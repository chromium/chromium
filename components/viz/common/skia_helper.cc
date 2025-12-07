// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/skia_helper.h"

#include <array>
#include <utility>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"

namespace viz {

sk_sp<SkColorFilter> SkiaHelper::MakeOverdrawColorFilter() {
#if BUILDFLAG(IS_ANDROID)
  static const SkColor colors[SkOverdrawColorFilter::kNumColors] = {
      0x00000000, 0x00000000, 0x2f0000ff, 0x2f00ff00, 0x3fff0000, 0x7fff0000,
  };
  return SkOverdrawColorFilter::MakeWithSkColors(colors);
#else
  constexpr int kNumColors = 9;
  static const std::array<SkColor, kNumColors> colors = {
      /*no-color=*/0x00000000,  /*no-color=*/0x00000000,
      /*blue=*/0x2f0000ff,      /*green=*/0x2f00ff00,
      /*light-red=*/0x3fff0000,
      /*red=*/0x7fff0000,       /*orange=*/0x9fff8c00,
      /*purple=*/0x9f6A0DAD,    /*wine=*/0x9f2C041C};
  static constexpr char kOverdrawFilterCode[] =
      "uniform half4 color0, color1, color2, color3, color4, color5, color6, "
      "color7, color8;"

      "half4 main(half4 color) {"
      "return color.a < (0.5 / 255.) ? color0"
      ": color.a < (1.5 / 255.) ? color1"
      ": color.a < (2.5 / 255.) ? color2"
      ": color.a < (3.5 / 255.) ? color3"
      ": color.a < (4.5 / 255.) ? color4"
      ": color.a < (5.5 / 255.) ? color5"
      ": color.a < (6.5 / 255.) ? color6"
      ": color.a < (7.5 / 255.) ? color7"
      ": color8;"
      "}";

  static auto* const overdraw_effect =
      SkRuntimeEffect::MakeForColorFilter(SkString(kOverdrawFilterCode))
          .effect.release();
  CHECK(overdraw_effect);

  SkRuntimeColorFilterBuilder builder(sk_ref_sp(overdraw_effect));
  for (int i = 0; i < kNumColors; i++) {
    builder.uniform(base::StrCat({"color", base::NumberToString(i)})) =
        SkColor4f::FromColor(colors[i]).premul();
  }

  return builder.makeColorFilter();
#endif
}

sk_sp<SkImageFilter> SkiaHelper::BuildOpacityFilter(float opacity) {
  SkColorMatrix matrix;
  matrix.setScale(1.f, 1.f, 1.f, opacity);
  return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix), nullptr);
}

}  // namespace viz
