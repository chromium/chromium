// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_UTIL_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_UTIL_H_

#include <string>

#include "components/viz/common/viz_common_export.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {

namespace copy_output {

// Returns the pixels in the scaled result coordinate space that are affected by
// the source |area| and scaling ratio. If application of the scaling ratio
// generates coordinates that are out-of-range or otherwise not "safely
// reasonable," an empty Rect is returned.
gfx::Rect VIZ_COMMON_EXPORT ComputeResultRect(const gfx::Rect& area,
                                              const gfx::Vector2d& scale_from,
                                              const gfx::Vector2d& scale_to);

// Geometry of the CopyOutputRequest mapped to the draw and window space of
// the relevant RenderPass.
struct VIZ_COMMON_EXPORT RenderPassGeometry {
  // Bounds CopyOutputRequest result. RenderPass output_rect clamped to
  // CopyOutputRequest area. Represented in post-scaled draw coordinate space.
  gfx::Rect result_bounds;

  // |result_bounds| clamped to the CopyOutputRequest selection. Represented in
  // post-scaled draw coordinate space. It is the region that is actually
  // returned.
  gfx::Rect result_selection;

  // |result_bounds| represented in pre-scaled window coordinate space.
  gfx::Rect sampling_bounds;

  // If request is not scaled, the origin of |result_selection| in window
  // coordinate space. Otherwise undefined.
  gfx::Vector2d readback_offset;

  RenderPassGeometry();
  ~RenderPassGeometry();

  std::string ToString() const;
};

}  // namespace copy_output
}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_UTIL_H_
