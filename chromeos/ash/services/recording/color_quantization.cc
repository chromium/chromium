// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/color_quantization.h"

#include <cstdint>

#include "base/check_op.h"
#include "chromeos/ash/services/recording/gif_encoding_types.h"
#include "chromeos/ash/services/recording/rgb_video_frame.h"

namespace recording {

namespace {

// If no color exists in the color table whose squared distance to the current
// color being added is smaller than this value, the new color will be added to
// the table.
constexpr uint32_t kMinSquaredDistanceToAddColor = 75;

// Defines an index of the closest color found in a color palette along with the
// squared Euclidean distance from the actual color.
struct FoundColorIndex {
  ColorIndex index;
  uint32_t squared_distance;
};

// Calculates and returns the squared value of the Euclidean distance between
// the two given colors.
uint32_t CalculateColorDistanceSquared(const RgbColor& color_a,
                                       const RgbColor& color_b) {
  const uint32_t diff_r = color_a.r - color_b.r;
  const uint32_t diff_g = color_a.g - color_b.g;
  const uint32_t diff_b = color_a.b - color_b.b;
  return diff_r * diff_r + diff_g * diff_g + diff_b * diff_b;
}

// Finds the closest color to the given `new_color` in the given
// `color_palette`. Note that if the palette is empty, the returned index of the
// found color will be -1.
FoundColorIndex FindClosestColor(const RgbColor& new_color,
                                 const ColorTable& color_palette) {
  int index_of_closest = -1;
  uint32_t min_squared_distance = std::numeric_limits<uint32_t>::max();
  const size_t current_size = color_palette.size();
  for (size_t i = 0; i < current_size; ++i) {
    const auto& current_color = color_palette[i];
    if (current_color == new_color) {
      // The exact color already exists in the palette.
      return FoundColorIndex{/*index=*/static_cast<ColorIndex>(i),
                             /*squared_distance=*/0};
    }

    const uint32_t squared_distance =
        CalculateColorDistanceSquared(new_color, current_color);
    if (squared_distance < min_squared_distance) {
      min_squared_distance = squared_distance;
      index_of_closest = i;
    }
  }

  return FoundColorIndex{static_cast<ColorIndex>(index_of_closest),
                         min_squared_distance};
}

// If `new_color` already exists in `out_palette`, it returns its index
// immediately. Otherwise, it tries to add it to the palette if possible (i.e.
// if there's still room in the palette and there's no other color that is
// considered close enough), and returns the index. If addition is not possible,
// it returns the index of the closest color in the palette.
ColorIndex MaybeAddColorToPalette(const RgbColor& new_color,
                                  ColorTable& out_palette) {
  FoundColorIndex result = FindClosestColor(new_color, out_palette);

  const size_t current_size = out_palette.size();
  if (current_size < kMaxNumberOfColorsInPalette &&
      result.squared_distance >= kMinSquaredDistanceToAddColor) {
    out_palette.push_back(new_color);
    return current_size;
  }

  DCHECK_NE(result.index, -1);
  return result.index;
}

// Invokes the given functor `f` on each pixel's `RgbColor` of the given
// `rgb_video_frame`.
template <class Functor>
void ForEachPixelColor(const RgbVideoFrame& rgb_video_frame, Functor f) {
  const RgbColor* pixel = &rgb_video_frame.pixel_color(0, 0);
  const RgbColor* const end = &pixel[rgb_video_frame.num_pixels()];
  for (; pixel < end; ++pixel) {
    const auto& color = *pixel;
    f(color);
  }
}

}  // namespace

// TODO(b/270604745): Implement a better color quantization algorithm.
void BuildColorPaletteAndPixelIndices(const RgbVideoFrame& rgb_video_frame,
                                      ColorTable& out_color_palette,
                                      ColorIndices& out_pixel_color_indices) {
  out_color_palette.clear();
  out_pixel_color_indices.clear();

  ForEachPixelColor(rgb_video_frame, [&](const RgbColor& color) {
    const ColorIndex index = MaybeAddColorToPalette(color, out_color_palette);
    DCHECK_GE(index, 0);
    DCHECK_LT(index, out_color_palette.size());
    out_pixel_color_indices.push_back(index);
  });

  DCHECK_LE(out_color_palette.size(), kMaxNumberOfColorsInPalette);
}

ColorTable BuildColorPalette(const RgbVideoFrame& rgb_video_frame) {
  ColorTable result;
  result.reserve(kMaxNumberOfColorsInPalette);

  ForEachPixelColor(rgb_video_frame, [&](const RgbColor& color) {
    MaybeAddColorToPalette(color, result);
  });

  DCHECK_LE(result.size(), kMaxNumberOfColorsInPalette);

  return result;
}

void BuildPixelIndices(const RgbVideoFrame& rgb_video_frame,
                       const ColorTable& color_palette,
                       ColorIndices& out_pixel_color_indices) {
  DCHECK(!color_palette.empty());
  out_pixel_color_indices.clear();

  ForEachPixelColor(rgb_video_frame, [&](const RgbColor& color) {
    const ColorIndex index = FindClosestColor(color, color_palette).index;
    DCHECK_GE(index, 0);
    DCHECK_LT(index, color_palette.size());
    out_pixel_color_indices.push_back(index);
  });
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
