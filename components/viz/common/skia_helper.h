// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SKIA_HELPER_H_
#define COMPONENTS_VIZ_COMMON_SKIA_HELPER_H_

#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {
class VIZ_COMMON_EXPORT SkiaHelper {
 public:
  static sk_sp<SkColorFilter> MakeOverdrawColorFilter();

  static sk_sp<SkImageFilter> BuildOpacityFilter(float opacity);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SKIA_HELPER_H_
