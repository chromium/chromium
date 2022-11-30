// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QR_CODE_GENERATOR_DINO_IMAGE_H_
#define COMPONENTS_QR_CODE_GENERATOR_DINO_IMAGE_H_

// Contains constants clients use to to render a dino on top of a QR image.
namespace dino_image {

// Width of the dino pixel data.
static constexpr int kDinoWidth = 20;
// Height of the dino pixel data.
static constexpr int kDinoHeight = 22;
// Height of the dino pixel data, head segment.
static constexpr int kDinoHeadHeight = 8;
// Height of the dino image data, body segment.
static constexpr int kDinoBodyHeight = kDinoHeight - kDinoHeadHeight;
// Width of the dino image data.
static constexpr int kDinoWidthBytes = (kDinoWidth + 7) / 8;

// Pixel data for the dino's head, facing right.
static const unsigned char kDinoHeadRight[kDinoWidthBytes * kDinoHeadHeight] = {
    // clang-format off
  0b00000000, 0b00011111, 0b11100000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00110111, 0b11110000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00111110, 0b00000000,
  0b00000000, 0b00111111, 0b11000000,
    // clang-format on
};

// Pixel data for the dino's head, facing left.
static const unsigned char kDinoHeadLeft[kDinoWidthBytes * kDinoHeadHeight] = {
    // clang-format off
  0b00000111, 0b11111000, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00001111, 0b11101100, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00000000, 0b01111100, 0b00000000,
  0b00000011, 0b11111100, 0b00000000,
    // clang-format on
};

// Pixel data for the dino's body.
static const unsigned char kDinoBody[kDinoWidthBytes * kDinoBodyHeight] = {
    // clang-format off
  0b10000000, 0b01111100, 0b00000000,
  0b10000001, 0b11111100, 0b00000000,
  0b11000011, 0b11111111, 0b00000000,
  0b11100111, 0b11111101, 0b00000000,
  0b11111111, 0b11111100, 0b00000000,
  0b11111111, 0b11111100, 0b00000000,
  0b01111111, 0b11111000, 0b00000000,
  0b00111111, 0b11111000, 0b00000000,
  0b00011111, 0b11110000, 0b00000000,
  0b00001111, 0b11100000, 0b00000000,
  0b00000111, 0b01100000, 0b00000000,
  0b00000110, 0b00100000, 0b00000000,
  0b00000100, 0b00100000, 0b00000000,
  0b00000110, 0b00110000, 0b00000000,
    // clang-format on
};

}  // namespace dino_image
#endif  // COMPONENTS_QR_CODE_GENERATOR_DINO_IMAGE_H_
