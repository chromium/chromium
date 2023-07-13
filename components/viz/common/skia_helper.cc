// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/viz/common/skia_helper.h"

#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"

namespace viz {

sk_sp<SkColorFilter> SkiaHelper::MakeOverdrawColorFilter() {
  static const SkColor colors[SkOverdrawColorFilter::kNumColors] = {
      0x00000000, 0x00000000, 0x2f0000ff, 0x2f00ff00, 0x3fff0000, 0x7fff0000,
  };
  return SkOverdrawColorFilter::MakeWithSkColors(colors);
}

sk_sp<SkImageFilter> SkiaHelper::BuildOpacityFilter(float opacity) {
  SkColorMatrix matrix;
  matrix.setScale(1.f, 1.f, 1.f, opacity);
  return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix), nullptr);
}

}  // namespace viz
