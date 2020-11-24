// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"

#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace viz {

AggregatedRenderPassDrawQuad::AggregatedRenderPassDrawQuad() = default;

AggregatedRenderPassDrawQuad::AggregatedRenderPassDrawQuad(
    const AggregatedRenderPassDrawQuad& other) = default;

AggregatedRenderPassDrawQuad::~AggregatedRenderPassDrawQuad() = default;

void AggregatedRenderPassDrawQuad::SetNew(
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    AggregatedRenderPassId render_pass_id,
    ResourceId mask_resource_id,
    const gfx::RectF& mask_uv_rect,
    const gfx::Size& mask_texture_size,
    const gfx::Vector2dF& filters_scale,
    const gfx::PointF& filters_origin,
    const gfx::RectF& tex_coord_rect,
    bool force_anti_aliasing_off,
    float backdrop_filter_quality) {
  DCHECK(render_pass_id);

  bool needs_blending = true;
  bool intersects_damage_under = true;
  SetAll(shared_quad_state, rect, visible_rect, needs_blending, render_pass_id,
         mask_resource_id, mask_uv_rect, mask_texture_size, filters_scale,
         filters_origin, tex_coord_rect, force_anti_aliasing_off,
         backdrop_filter_quality, intersects_damage_under);
}

void AggregatedRenderPassDrawQuad::SetAll(
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    bool needs_blending,
    AggregatedRenderPassId render_pass_id,
    ResourceId mask_resource_id,
    const gfx::RectF& mask_uv_rect,
    const gfx::Size& mask_texture_size,
    const gfx::Vector2dF& filters_scale,
    const gfx::PointF& filters_origin,
    const gfx::RectF& tex_coord_rect,
    bool force_anti_aliasing_off,
    float backdrop_filter_quality,
    bool intersects_damage_under) {
  DCHECK(render_pass_id);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kAggregatedRenderPass,
                   rect, visible_rect, needs_blending);
  this->render_pass_id = render_pass_id;
  resources.ids[kMaskResourceIdIndex] = mask_resource_id;
  resources.count = mask_resource_id ? 1 : 0;
  this->mask_uv_rect = mask_uv_rect;
  this->mask_texture_size = mask_texture_size;
  this->filters_scale = filters_scale;
  this->filters_origin = filters_origin;
  this->tex_coord_rect = tex_coord_rect;
  this->force_anti_aliasing_off = force_anti_aliasing_off;
  this->backdrop_filter_quality = backdrop_filter_quality;
  this->intersects_damage_under = intersects_damage_under;
}

const AggregatedRenderPassDrawQuad* AggregatedRenderPassDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::Material::kAggregatedRenderPass);
  return static_cast<const AggregatedRenderPassDrawQuad*>(quad);
}

void AggregatedRenderPassDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  TracedValue::SetIDRef(
      reinterpret_cast<void*>(static_cast<uint64_t>(render_pass_id)), value,
      "render_pass_id");
  RenderPassDrawQuadInternal::ExtendValue(value);
}

}  // namespace viz
