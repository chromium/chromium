// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_DRAW_QUAD_INTERNAL_H_
#define COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_DRAW_QUAD_INTERNAL_H_

#include <stddef.h>

#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {

class VIZ_COMMON_EXPORT RenderPassDrawQuadInternal : public DrawQuad {
 public:
  void SetFilters(cc::FilterOperations filters,
                  cc::FilterOperations backdrop_filters,
                  std::optional<SkPath> backdrop_filter_bounds,
                  const gfx::Vector2dF& filters_scale,
                  const gfx::PointF& filters_origin,
                  const float backdrop_filter_quality);

  gfx::RectF mask_uv_rect;
  gfx::Size mask_texture_size;

  bool force_anti_aliasing_off = false;

  // Indicates if this quad intersects any damage from quads under it rendering
  // to the same target.
  mutable bool intersects_damage_under = true;

  // Post-processing filters, applied to the pixels in the render pass' texture.
  cc::FilterOperations filters;

  // Post-processing filters, applied to the pixels showing through the
  // backdrop of the render pass, from behind it.
  cc::FilterOperations backdrop_filters;

  // Clipping bounds for backdrop filter. If defined, is in a coordinate space
  // equivalent to render pass physical pixels after applying
  // `RenderPassDrawQuad::filter_scale`.
  std::optional<SkPath> backdrop_filter_bounds;

  // The scale from layer space of the root layer of the render pass to
  // the render pass physical pixels. This scale is applied to the filter
  // parameters for pixel-moving filters. This scale should include
  // content-to-target-space scale, and device pixel ratio.
  gfx::Vector2dF filters_scale{1.0f, 1.0f};

  // The origin for post-processing filters which will be used to offset
  // crop rects, lights, etc.
  gfx::PointF filters_origin;

  float backdrop_filter_quality = 1.0f;

  ResourceId mask_resource_id() const { return resource_id; }

 protected:
  RenderPassDrawQuadInternal();
  RenderPassDrawQuadInternal(const RenderPassDrawQuadInternal& other);
  ~RenderPassDrawQuadInternal() override;

  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_DRAW_QUAD_INTERNAL_H_
