// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_DRAW_QUAD_H_

#include <stddef.h>

#include <memory>

#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/viz_common_export.h"

#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {

class VIZ_COMMON_EXPORT RenderPassDrawQuad : public DrawQuad {
 public:
  static const size_t kMaskResourceIdIndex = 0;

  RenderPassDrawQuad();
  RenderPassDrawQuad(const RenderPassDrawQuad& other);
  ~RenderPassDrawQuad() override;

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              RenderPassId render_pass_id,
              ResourceId mask_resource_id,
              const gfx::RectF& mask_uv_rect,
              const gfx::Size& mask_texture_size,
              const gfx::Vector2dF& filters_scale,
              const gfx::PointF& filters_origin,
              const gfx::RectF& tex_coord_rect,
              bool force_anti_aliasing_off,
              float backdrop_filter_quality);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              RenderPassId render_pass_id,
              ResourceId mask_resource_id,
              const gfx::RectF& mask_uv_rect,
              const gfx::Size& mask_texture_size,
              const gfx::Vector2dF& filters_scale,
              const gfx::PointF& filters_origin,
              const gfx::RectF& tex_coord_rect,
              bool force_anti_aliasing_off,
              float backdrop_filter_quality);

  RenderPassId render_pass_id;
  gfx::RectF mask_uv_rect;
  gfx::Size mask_texture_size;

  // The scale from layer space of the root layer of the render pass to
  // the render pass physical pixels. This scale is applied to the filter
  // parameters for pixel-moving filters. This scale should include
  // content-to-target-space scale, and device pixel ratio.
  gfx::Vector2dF filters_scale;

  // The origin for post-processing filters which will be used to offset
  // crop rects, lights, etc.
  gfx::PointF filters_origin;

  gfx::RectF tex_coord_rect;

  float backdrop_filter_quality;

  bool force_anti_aliasing_off;

  ResourceId mask_resource_id() const {
    return resources.ids[kMaskResourceIdIndex];
  }

  static const RenderPassDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_DRAW_QUAD_H_
