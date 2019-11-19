// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/viz/common/skia_helper.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/skia_util.h"

namespace viz {
sk_sp<SkImage> SkiaHelper::ApplyImageFilter(GrContext* context,
                                            sk_sp<SkImage> src_image,
                                            const gfx::RectF& src_rect,
                                            const gfx::RectF& dst_rect,
                                            const gfx::Vector2dF& scale,
                                            sk_sp<SkImageFilter> filter,
                                            SkIPoint* offset,
                                            SkIRect* subset,
                                            const gfx::PointF& origin,
                                            bool flush) {
  if (!filter)
    return nullptr;

  if (!src_image) {
    TRACE_EVENT_INSTANT0("viz",
                         "ApplyImageFilter wrap background texture failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return nullptr;
  }

  // Big filters can sometimes fallback to CPU. Therefore, we need
  // to disable subnormal floats for performance and security reasons.
  cc::ScopedSubnormalFloatDisabler disabler;
  SkMatrix local_matrix;
  local_matrix.setTranslate(origin.x(), origin.y());
  local_matrix.postScale(scale.x(), scale.y());
  local_matrix.postTranslate(-src_rect.x(), -src_rect.y());

  SkIRect clip_bounds = gfx::RectFToSkRect(dst_rect).roundOut();
  clip_bounds.offset(-src_rect.x(), -src_rect.y());

  filter = filter->makeWithLocalMatrix(local_matrix);
  SkIRect in_subset = SkIRect::MakeWH(src_rect.width(), src_rect.height());

  sk_sp<SkImage> image = src_image->makeWithFilter(
      context, filter.get(), in_subset, clip_bounds, subset, offset);
  if (!image || !image->isTextureBacked()) {
    return nullptr;
  }

  // Force a flush of the Skia pipeline before we switch back to the compositor
  // context.
  image->getBackendTexture(flush);
  CHECK(image->isTextureBacked());
  return image;
}

sk_sp<SkColorFilter> SkiaHelper::MakeOverdrawColorFilter() {
  // TODO(xing.xu) : handle this in CPU mode, the R and B should be
  // switched in CPU mode. (http://crbug.com/896969)
  static const SkPMColor colors[SkOverdrawColorFilter::kNumColors] = {
      0x00000000, 0x00000000, 0x2fff0000, 0x2f00ff00, 0x3f0000ff, 0x7f0000ff,
  };
  return SkOverdrawColorFilter::Make(colors);
}

sk_sp<SkImageFilter> SkiaHelper::BuildOpacityFilter(float opacity) {
  SkColorMatrix matrix;
  matrix.setScale(1.f, 1.f, 1.f, opacity);
  return SkColorFilterImageFilter::Make(SkColorFilters::Matrix(matrix),
                                        nullptr);
}

}  // namespace viz
