// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_AGGREGATED_RENDER_PASS_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_AGGREGATED_RENDER_PASS_DRAW_QUAD_H_

#include <stddef.h>

#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad_internal.h"
#include "components/viz/common/viz_common_export.h"

#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {

class VIZ_COMMON_EXPORT AggregatedRenderPassDrawQuad
    : public RenderPassDrawQuadInternal {
 public:
  static constexpr Material kMaterial = Material::kAggregatedRenderPass;

  AggregatedRenderPassDrawQuad();
  AggregatedRenderPassDrawQuad(const AggregatedRenderPassDrawQuad& other);
  ~AggregatedRenderPassDrawQuad() override;

  // No comparison for resources.ids. shared_quad_state is compared
  // by calling Equals().
  bool Equals(const AggregatedRenderPassDrawQuad& other) const;

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              AggregatedRenderPassId render_pass,
              ResourceId mask_resource_id,
              const gfx::RectF& mask_uv_rect,
              const gfx::Size& mask_texture_size,
              const gfx::Vector2dF& filters_scale,
              const gfx::PointF& filters_origin,
              const gfx::RectF& tex_coord_rect,
              bool force_anti_aliasing_off,
              float backdrop_filter_quality);

  void SetAll(const AggregatedRenderPassDrawQuad& other);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              AggregatedRenderPassId render_pass,
              ResourceId mask_resource_id,
              const gfx::RectF& mask_uv_rect,
              const gfx::Size& mask_texture_size,
              const gfx::Vector2dF& filters_scale,
              const gfx::PointF& filters_origin,
              const gfx::RectF& tex_coord_rect,
              bool force_anti_aliasing_off,
              float backdrop_filter_quality,
              bool intersects_damage_under);

  AggregatedRenderPassId render_pass_id;

  static const AggregatedRenderPassDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_AGGREGATED_RENDER_PASS_DRAW_QUAD_H_
