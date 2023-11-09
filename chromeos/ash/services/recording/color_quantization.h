// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_COLOR_QUANTIZATION_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_COLOR_QUANTIZATION_H_

#include "chromeos/ash/services/recording/gif_encoding_types.h"

namespace recording {

class RgbVideoFrame;

// GIF images can have a maximum number of 256 colors in their color tables.
// This means that the minimum number of bits needed to represent this count is
// 8, which is the max bit depth value.
constexpr size_t kMaxNumberOfColorsInPalette = 256;
constexpr uint8_t kMaxColorBitDepth = 8;

// TODO(http://b/308444948): This code is going away soon and will be replaced
// with a more efficient Octree-based color quantizer.

// Performs color quantization on the given `rgb_video_frame` and fills
// `out_color_palette` with the most important 256 colors in the image, and also
// fills `out_pixel_color_indices` with the indices of the chosen colors from
// `out_color_palette` for all the pixels in `rgb_video_frame`.
void BuildColorPaletteAndPixelIndices(const RgbVideoFrame& rgb_video_frame,
                                      ColorTable& out_color_palette,
                                      ColorIndices& out_pixel_color_indices);

// Extracts a color palette from the given `rgb_video_frame` and returns it.
ColorTable BuildColorPalette(const RgbVideoFrame& rgb_video_frame);

// For each pixel in the given `rgb_video_frame`, fills in
// `out_pixel_color_indices` with an index of a color in the given
// `color_palette`.
void BuildPixelIndices(const RgbVideoFrame& rgb_video_frame,
                       const ColorTable& color_palette,
                       ColorIndices& out_pixel_color_indices);

// Calculates and returns the color bit depth based on the size of the given
// `color_palette`. The color bit depth is the least number of bits needed to be
// able to represent the size of the palette as a binary number.
uint8_t CalculateColorBitDepth(const ColorTable& color_palette);

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_COLOR_QUANTIZATION_H_
