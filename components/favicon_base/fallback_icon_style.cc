// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/fallback_icon_style.h"

#include <algorithm>

#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace favicon_base {

namespace {

// The maximum lightness of the background color to ensure light text is
// readable.
const double kMaxBackgroundColorLightness = 0.67;
const double kMinBackgroundColorLightness = 0.15;

// Default values for FallbackIconStyle.
const SkColor kDefaultBackgroundColor = SkColorSetRGB(0x78, 0x78, 0x78);
const SkColor kDefaultTextColor = SK_ColorWHITE;

}  // namespace

FallbackIconStyle::FallbackIconStyle()
    : background_color(kDefaultBackgroundColor),
      is_default_background_color(true),
      text_color(kDefaultTextColor) {}

FallbackIconStyle::~FallbackIconStyle() {
}

bool FallbackIconStyle::operator==(const FallbackIconStyle& other) const {
  return background_color == other.background_color &&
         is_default_background_color == other.is_default_background_color &&
         text_color == other.text_color;
}

void SetDominantColorAsBackground(base::span<const uint8_t> bitmap_data,
                                  FallbackIconStyle* style) {
  // Try to ensure color's lightness isn't too large so that light text is
  // visible. Set an upper bound for the dominant color.
  const color_utils::HSL lower_bound{-1.0, -1.0, kMinBackgroundColorLightness};
  const color_utils::HSL upper_bound{-1.0, -1.0, kMaxBackgroundColorLightness};
  color_utils::GridSampler sampler;
  SkColor dominant_color = color_utils::CalculateKMeanColorOfPNG(
      bitmap_data, lower_bound, upper_bound, &sampler);
  // |CalculateKMeanColorOfPNG| will try to return a color that lies within the
  // specified bounds if one exists in the image. If there's no such color, it
  // will return the dominant color which may be lighter than our upper bound.
  // Clamp lightness down to a reasonable maximum value so text is readable.
  color_utils::HSL color_hsl;
  color_utils::SkColorToHSL(dominant_color, &color_hsl);
  color_hsl.l = std::min(color_hsl.l, kMaxBackgroundColorLightness);
  style->background_color =
      color_utils::HSLToSkColor(color_hsl, SK_AlphaOPAQUE);
  style->is_default_background_color = false;
}

}  // namespace favicon_base
