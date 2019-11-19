// Copyright (c) 2018 The Chromium Authors. All rights reserved.
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
  // |flush| is necessary for GLRenderer but not SkiaRenderer.
  static sk_sp<SkImage> ApplyImageFilter(GrContext* context,
                                         sk_sp<SkImage> src_image,
                                         const gfx::RectF& src_rect,
                                         const gfx::RectF& dst_rect,
                                         const gfx::Vector2dF& scale,
                                         sk_sp<SkImageFilter> filter,
                                         SkIPoint* offset,
                                         SkIRect* subset,
                                         const gfx::PointF& origin,
                                         bool flush);

  static sk_sp<SkColorFilter> MakeOverdrawColorFilter();

  static sk_sp<SkImageFilter> BuildOpacityFilter(float opacity);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SKIA_HELPER_H_
