// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_geometry_util.h"

#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

// This code is adapted from the GVR Treasure Hunt demo source.
gfx::Rect CalculatePixelSpaceRect(const gfx::Size& texture_size,
                                  const gfx::RectF& texture_rect) {
  const gfx::RectF rect =
      ScaleRect(texture_rect, static_cast<float>(texture_size.width()),
                static_cast<float>(texture_size.height()));
  return gfx::Rect(rect.x(), rect.y(), rect.width(), rect.height());
}

}  // namespace vr
