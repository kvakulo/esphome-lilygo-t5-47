// Source: https://github.com/ashald/platformio-epdiy-monochrome/
// License: The Unlicense (https://github.com/ashald/platformio-epdiy-monochrome/blob/main/LICENSE)

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "eink.h"
#include "esphome/core/log.h"

extern "C" {
#include "display_ops.h"
#include "epd_driver.h"
#include "lut.h"
}

#define WHITE 0b10101010 // 0xAA
#define BLACK 0b01010101 // 0x55

static const char *const TAG = "lilygo_t5_47.display";

static const uint8_t ROW_DELAY = 120;
static const uint8_t UPDATE_CYCLES = 16;

static const uint8_t BUFFER_BLACK = 0xFF;
static const uint8_t BUFFER_WHITE = 0x00;

// 0..255, in binary form, 0s replaced with WHITE, and 1s with BLACK
static const uint16_t lookup[] = {
    0xAAAA, 0xAAA9, 0xAAA6, 0xAAA5, 0xAA9A, 0xAA99, 0xAA96, 0xAA95, 0xAA6A,
    0xAA69, 0xAA66, 0xAA65, 0xAA5A, 0xAA59, 0xAA56, 0xAA55, 0xA9AA, 0xA9A9,
    0xA9A6, 0xA9A5, 0xA99A, 0xA999, 0xA996, 0xA995, 0xA96A, 0xA969, 0xA966,
    0xA965, 0xA95A, 0xA959, 0xA956, 0xA955, 0xA6AA, 0xA6A9, 0xA6A6, 0xA6A5,
    0xA69A, 0xA699, 0xA696, 0xA695, 0xA66A, 0xA669, 0xA666, 0xA665, 0xA65A,
    0xA659, 0xA656, 0xA655, 0xA5AA, 0xA5A9, 0xA5A6, 0xA5A5, 0xA59A, 0xA599,
    0xA596, 0xA595, 0xA56A, 0xA569, 0xA566, 0xA565, 0xA55A, 0xA559, 0xA556,
    0xA555, 0x9AAA, 0x9AA9, 0x9AA6, 0x9AA5, 0x9A9A, 0x9A99, 0x9A96, 0x9A95,
    0x9A6A, 0x9A69, 0x9A66, 0x9A65, 0x9A5A, 0x9A59, 0x9A56, 0x9A55, 0x99AA,
    0x99A9, 0x99A6, 0x99A5, 0x999A, 0x9999, 0x9996, 0x9995, 0x996A, 0x9969,
    0x9966, 0x9965, 0x995A, 0x9959, 0x9956, 0x9955, 0x96AA, 0x96A9, 0x96A6,
    0x96A5, 0x969A, 0x9699, 0x9696, 0x9695, 0x966A, 0x9669, 0x9666, 0x9665,
    0x965A, 0x9659, 0x9656, 0x9655, 0x95AA, 0x95A9, 0x95A6, 0x95A5, 0x959A,
    0x9599, 0x9596, 0x9595, 0x956A, 0x9569, 0x9566, 0x9565, 0x955A, 0x9559,
    0x9556, 0x9555, 0x6AAA, 0x6AA9, 0x6AA6, 0x6AA5, 0x6A9A, 0x6A99, 0x6A96,
    0x6A95, 0x6A6A, 0x6A69, 0x6A66, 0x6A65, 0x6A5A, 0x6A59, 0x6A56, 0x6A55,
    0x69AA, 0x69A9, 0x69A6, 0x69A5, 0x699A, 0x6999, 0x6996, 0x6995, 0x696A,
    0x6969, 0x6966, 0x6965, 0x695A, 0x6959, 0x6956, 0x6955, 0x66AA, 0x66A9,
    0x66A6, 0x66A5, 0x669A, 0x6699, 0x6696, 0x6695, 0x666A, 0x6669, 0x6666,
    0x6665, 0x665A, 0x6659, 0x6656, 0x6655, 0x65AA, 0x65A9, 0x65A6, 0x65A5,
    0x659A, 0x6599, 0x6596, 0x6595, 0x656A, 0x6569, 0x6566, 0x6565, 0x655A,
    0x6559, 0x6556, 0x6555, 0x5AAA, 0x5AA9, 0x5AA6, 0x5AA5, 0x5A9A, 0x5A99,
    0x5A96, 0x5A95, 0x5A6A, 0x5A69, 0x5A66, 0x5A65, 0x5A5A, 0x5A59, 0x5A56,
    0x5A55, 0x59AA, 0x59A9, 0x59A6, 0x59A5, 0x599A, 0x5999, 0x5996, 0x5995,
    0x596A, 0x5969, 0x5966, 0x5965, 0x595A, 0x5959, 0x5956, 0x5955, 0x56AA,
    0x56A9, 0x56A6, 0x56A5, 0x569A, 0x5699, 0x5696, 0x5695, 0x566A, 0x5669,
    0x5666, 0x5665, 0x565A, 0x5659, 0x5656, 0x5655, 0x55AA, 0x55A9, 0x55A6,
    0x55A5, 0x559A, 0x5599, 0x5596, 0x5595, 0x556A, 0x5569, 0x5566, 0x5565,
    0x555A, 0x5559, 0x5556, 0x5555,
};

static void eink_set_bit(uint32_t index, bool value, uint8_t* buffer) {
  uint32_t byte = index / 8;
  uint32_t bit = index % 8;
  uint8_t mask = 1 << bit;

  if (value) {
    buffer[byte] |= mask;
  } else {
    buffer[byte] &= ~mask;
  }
}

static void eink_fill(uint8_t color) {
  uint8_t row[EPD_LINE_BYTES];
  memset(row, color, EPD_LINE_BYTES);

  reorder_line_buffer((uint32_t*)row);

  epd_start_frame();

  for (int i = 0; i < EPD_HEIGHT; i++) {
    epd_switch_buffer();
    memcpy(epd_get_current_buffer(), row, EPD_LINE_BYTES);

    write_row(ROW_DELAY);
  }

  write_row(ROW_DELAY);

  epd_end_frame();
}

static void eink_draw(uint8_t* buffer, bool inverse) {

  uint16_t row[EPD_LINE_BYTES/2] = {0};

  epd_start_frame();

  for (uint32_t y = 0; y < EPD_HEIGHT; y++) {
    memset(row, 0, EPD_LINE_BYTES);

    // 1x8PpB = 2x4PpB = 1 word
    for (uint32_t word = 0; word < EPD_LINE_BYTES/2; word++) {
      uint8_t pixels_1bpp = buffer[y * EPD_LINE_BYTES/2 + word];
      uint8_t lookup_index = inverse ? 255 - pixels_1bpp : pixels_1bpp;
      uint16_t pixels_2bpp = lookup[lookup_index];  // translate into 2-bits-per-pixel
      row[word] = pixels_2bpp;
    }

    reorder_line_buffer((uint32_t*)row);

    epd_switch_buffer();
    memcpy(epd_get_current_buffer(), row, EPD_LINE_BYTES);
    epd_switch_buffer();
    memcpy(epd_get_current_buffer(), row, EPD_LINE_BYTES);

    write_row(ROW_DELAY);
  }


  write_row(ROW_DELAY);

  epd_end_frame();
}

void eink_buffer_clear(uint8_t* buffer) {
  memset(buffer, BUFFER_WHITE, EINK_BUFFER_SIZE);
}

void eink_buffer_set(uint8_t* buffer, bool value) {
  uint8_t color = value ? BUFFER_BLACK : BUFFER_WHITE;
  memset(buffer, color, EINK_BUFFER_SIZE);
}

void eink_set_pixel(uint32_t x, uint32_t y, bool value, uint8_t* buffer) {
  eink_set_bit(y * EPD_WIDTH + x, value, buffer);
}

void eink_flush(bool value) {
  uint8_t color = value ? BLACK : WHITE;
  for (uint8_t i = 0; i < UPDATE_CYCLES; i++) {
    eink_fill(color);
  }
}

void eink_render(uint8_t* buffer) {
  for (uint8_t i = 0; i < UPDATE_CYCLES; i++) {
    eink_draw(buffer, false);
  }
}

void eink_render_advanced(uint8_t* buffer, uint8_t cycles, bool inverse) {
  for (uint8_t i = 0; i < cycles; i++) {
    eink_draw(buffer, inverse);
  }
}

void eink_init() { epd_init(EPD_LUT_1K); }

void eink_power_on() { epd_poweron(); }

void eink_power_off() { epd_poweroff(); }

void eink_deinit() { epd_deinit(); }
