// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/color_quantization.h"

#include <cstdint>

#include "base/check_op.h"
#include "chromeos/ash/services/recording/gif_encoding_types.h"

namespace recording {

namespace {

// If no color exists in the color table whose squared distance to the current
// color being added is smaller than this value, the new color will be added to
// the table.
constexpr uint32_t kMinSquaredDistanceToAddColor = 75;

// Calculates and returns the squared value of the Euclidean distance between
// the two given colors.
uint32_t CalculateColorDistanceSquared(const SkColor& color_a,
                                       const SkColor& color_b) {
  const uint32_t diff_r = SkColorGetR(color_a) - SkColorGetR(color_b);
  const uint32_t diff_g = SkColorGetG(color_a) - SkColorGetG(color_b);
  const uint32_t diff_b = SkColorGetB(color_a) - SkColorGetB(color_b);
  return diff_r * diff_r + diff_g * diff_g + diff_b * diff_b;
}

// If `new_color` already exists in `out_palette`, it returns its index
// immediately. Otherwise, it tries to add it to the palette if possible (i.e.
// if there's still room in the palette and there's no other color that is
// considered close enough), and returns the index. If addition is not possible,
// it returns the index of the closest color in the palette.
ColorIndex MaybeAddColorToPalette(const SkColor& new_color,
                                  ColorTable& out_palette) {
  int index_of_closest = -1;
  uint32_t min_squared_distance = std::numeric_limits<uint32_t>::max();
  const size_t current_size = out_palette.size();
  for (size_t i = 0; i < current_size; ++i) {
    const auto& current_color = out_palette[i];
    if (current_color == new_color) {
      return i;
    }

    const uint32_t squared_distance =
        CalculateColorDistanceSquared(new_color, current_color);
    if (squared_distance < min_squared_distance) {
      min_squared_distance = squared_distance;
      index_of_closest = i;
    }
  }

  if (current_size < kMaxNumberOfColorsInPalette &&
      min_squared_distance >= kMinSquaredDistanceToAddColor) {
    out_palette.push_back(new_color);
    return current_size;
  }

  DCHECK_NE(index_of_closest, -1);
  return index_of_closest;
}

}  // namespace

// TODO(b/270604745): Implement a better color quantization algorithm.
void BuildColorPaletteAndPixelIndices(const SkBitmap& bitmap,
                                      ColorTable& out_color_palette,
                                      ColorIndices& out_pixel_color_indices) {
  out_color_palette.clear();
  out_pixel_color_indices.clear();

  for (int row = 0; row < bitmap.height(); ++row) {
    for (int col = 0; col < bitmap.width(); ++col) {
      // We do not care about the alpha values, since our palette contains only
      // RGB colors, therefore we make the color as fully opaque before calling
      // `MaybeAddColorToPalette()` to make color comparison with the `==`
      // operator possible.
      const auto color = SkColorSetA(bitmap.getColor(col, row), 0xFF);
      const ColorIndex index = MaybeAddColorToPalette(color, out_color_palette);
      DCHECK_GE(index, 0);
      DCHECK_LT(index, out_color_palette.size());
      out_pixel_color_indices.push_back(index);
    }
  }

  DCHECK_LE(out_color_palette.size(), kMaxNumberOfColorsInPalette);
}

uint8_t CalculateColorBitDepth(const ColorTable& color_palette) {
  DCHECK_LE(color_palette.size(), kMaxNumberOfColorsInPalette);

  uint8_t bit_depth = 1;
  while ((1u << bit_depth) < color_palette.size()) {
    ++bit_depth;
  }

  DCHECK_LE(bit_depth, kMaxColorBitDepth);
  return bit_depth;
}

}  // namespace recording
