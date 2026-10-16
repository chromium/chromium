// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"

#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace viz {

CompositorRenderPassDrawQuad::CompositorRenderPassDrawQuad() = default;

CompositorRenderPassDrawQuad::CompositorRenderPassDrawQuad(
    const CompositorRenderPassDrawQuad& other) = default;

CompositorRenderPassDrawQuad::~CompositorRenderPassDrawQuad() = default;

void CompositorRenderPassDrawQuad::SetNew(
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    CompositorRenderPassId render_pass,
    ResourceId mask_resource_id,
    const gfx::RectF& mask_uv_rect,
    const gfx::Size& mask_texture_size,
    bool force_anti_aliasing_off) {
  DCHECK(render_pass);

  bool needs_blending = true;
  bool intersects_damage_under = true;
  gfx::Vector2dF filters_scale = gfx::Vector2dF(1.0f, 1.0f);
  gfx::PointF filters_origin = gfx::PointF();
  float backdrop_filter_quality = 1.0f;
  cc::FilterOperations filters = cc::FilterOperations();
  cc::FilterOperations backdrop_filters = cc::FilterOperations();
  std::optional<SkPath> backdrop_filter_bounds;
  SetAll(shared_quad_state, rect, visible_rect, needs_blending, render_pass,
         mask_resource_id, mask_uv_rect, mask_texture_size, filters_scale,
         filters_origin, force_anti_aliasing_off, backdrop_filter_quality,
         intersects_damage_under, filters, backdrop_filters,
         backdrop_filter_bounds);
}

void CompositorRenderPassDrawQuad::SetAll(
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    bool needs_blending,
    CompositorRenderPassId render_pass,
    ResourceId mask_resource_id,
    const gfx::RectF& mask_uv_rect,
    const gfx::Size& mask_texture_size,
    const gfx::Vector2dF& filters_scale,
    const gfx::PointF& filters_origin,
    bool force_anti_aliasing_off,
    float backdrop_filter_quality,
    bool intersects_damage_under,
    const cc::FilterOperations& pass_filters,
    const cc::FilterOperations& pass_backdrop_filters,
    const std::optional<SkPath>& pass_backdrop_filter_bounds) {
  DCHECK(render_pass);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kCompositorRenderPass,
                   rect, visible_rect, needs_blending);
  render_pass_id = render_pass;
  resource_id = mask_resource_id;
  this->mask_uv_rect = mask_uv_rect;
  this->mask_texture_size = mask_texture_size;
  this->filters_scale = filters_scale;
  this->filters_origin = filters_origin;
  this->force_anti_aliasing_off = force_anti_aliasing_off;
  this->backdrop_filter_quality = backdrop_filter_quality;
  this->intersects_damage_under = intersects_damage_under;
  this->filters = pass_filters;
  this->backdrop_filters = pass_backdrop_filters;
  this->backdrop_filter_bounds = pass_backdrop_filter_bounds;
}

const CompositorRenderPassDrawQuad* CompositorRenderPassDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  CHECK_EQ(quad->material, DrawQuad::Material::kCompositorRenderPass);
  return static_cast<const CompositorRenderPassDrawQuad*>(quad);
}

void CompositorRenderPassDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  // render_pass_id.value() is a 64-bit uint even on 32-bit architectures, so
  // using reinterpret_cast for the intentional conversion to a TracedValue::Id.
  TracedValue::SetIDRef(
      TracedValue::Id(reinterpret_cast<void*>(render_pass_id.value())), value,
      "render_pass_id");
  RenderPassDrawQuadInternal::ExtendValue(value);
}

}  // namespace viz
