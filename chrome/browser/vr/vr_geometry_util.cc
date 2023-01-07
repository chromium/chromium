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

gfx::SizeF CalculateScreenSize(const gfx::Transform& proj_matrix,
                               float distance,
                               const gfx::SizeF& size) {
  // View matrix is the identity, thus, not needed in the calculation.
  gfx::Transform scale_transform;
  scale_transform.Scale(size.width(), size.height());

  gfx::Transform translate_transform;
  translate_transform.Translate3d(0, 0, -distance);

  gfx::Transform model_view_proj_matrix =
      proj_matrix * translate_transform * scale_transform;

  gfx::Point3F projected_upper_right_corner =
      model_view_proj_matrix.MapPoint(gfx::Point3F(0.5f, 0.5f, 0.0f));
  gfx::Point3F projected_lower_left_corner =
      model_view_proj_matrix.MapPoint(gfx::Point3F(-0.5f, -0.5f, 0.0f));

  // Calculate and return the normalized size in screen space.
  return gfx::SizeF((std::abs(projected_upper_right_corner.x()) +
                     std::abs(projected_lower_left_corner.x())) /
                        2.0f,
                    (std::abs(projected_upper_right_corner.y()) +
                     std::abs(projected_lower_left_corner.y())) /
                        2.0f);
}

}  // namespace vr
