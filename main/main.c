/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const int TRIG = 16;
const int ECHO = 18;
volatile absolute_time_t start_time, end_time;

QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;
SemaphoreHandle_t xSemaphoreTrigger;

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void echo_irq_handler(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE) {
        start_time = get_absolute_time();
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        end_time = get_absolute_time();
        int64_t pulse_time = absolute_time_diff_us(start_time, end_time);
        xQueueSend(xQueueTime, &pulse_time, 0); // wnvia tempo para a fila
    }
}

void trigger_task(void *pvParameters) {
    while (true) {
        gpio_put(TRIG, 1);
        sleep_us(10);
        gpio_put(TRIG, 0);
        xSemaphoreGive(xSemaphoreTrigger); // sinaliza a echo_task
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms
    }
}

void echo_task(void *pvParameters) {
    while (true) {
        xSemaphoreTake(xSemaphoreTrigger, portMAX_DELAY);
        int64_t pulse_time;
        float distance = -1.0; // Valor padrão de falha

        if (xQueueReceive(xQueueTime, &pulse_time, pdMS_TO_TICKS(38))) {
            distance = (pulse_time * 0.0343) / 2;
            
            if (distance > 400.0 || distance < 2.0) { // limites
                distance = -1.0;
            }
        }
        xQueueSend(xQueueDistance, &distance, 0);
    }
}

void oled_task(void *pvParameters) {
   
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    float distance;
    char display_str[20];

    while (true) {
        if (xQueueReceive(xQueueDistance, &distance, portMAX_DELAY)) {
            gfx_clear_buffer(&disp);
            
            if (distance >= 0) {
                //exibe a distância
                snprintf(display_str, sizeof(display_str), "Dist: %.2f cm", distance);
            } else {
                // falha
                snprintf(display_str, sizeof(display_str), "Falha");
            }
            
            gfx_draw_string(&disp, 0, 0, 1, display_str);
            gfx_show(&disp);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay 
    }
}

int main() {
    stdio_init_all();
    gpio_init(TRIG);
    gpio_init(ECHO);
    gpio_set_dir(TRIG, GPIO_OUT);
    gpio_set_dir(ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_irq_handler);
    
    //  recursos
    xQueueTime = xQueueCreate(1, sizeof(int64_t));
    xQueueDistance = xQueueCreate(1, sizeof(float));
    xSemaphoreTrigger = xSemaphoreCreateBinary();
    
    // tasks
    xTaskCreate(trigger_task, "Trigger", 128, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 128, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 128, NULL, 1, NULL);
    
    vTaskStartScheduler(); 
    
    while (true); 
}
