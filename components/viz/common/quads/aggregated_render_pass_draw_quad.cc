// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

bool AggregatedRenderPassDrawQuad::Equals(
    const AggregatedRenderPassDrawQuad& other) const {
  DCHECK(shared_quad_state);

  return render_pass_id == other.render_pass_id &&
         // RenderPassDrawQuadInternal
         mask_uv_rect == other.mask_uv_rect &&
         mask_texture_size == other.mask_texture_size &&
         filters_scale == other.filters_scale &&
         filters_origin == other.filters_origin &&
         tex_coord_rect == other.tex_coord_rect &&
         backdrop_filter_quality == other.backdrop_filter_quality &&
         force_anti_aliasing_off == other.force_anti_aliasing_off &&
         intersects_damage_under == other.intersects_damage_under &&
         // DrawQuad. Skip resources.ids[kMaskResourceIdIndex].
         material == other.material && rect == other.rect &&
         visible_rect == other.visible_rect &&
         needs_blending == other.needs_blending &&
         shared_quad_state->Equals(*other.shared_quad_state);
}

void AggregatedRenderPassDrawQuad::SetNew(
    const SharedQuadState* shared_quad_state,
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
    float backdrop_filter_quality) {
  DCHECK(render_pass);

  bool needs_blending = true;
  bool intersects_damage_under = true;
  SetAll(shared_quad_state, rect, visible_rect, needs_blending, render_pass,
         mask_resource_id, mask_uv_rect, mask_texture_size, filters_scale,
         filters_origin, tex_coord_rect, force_anti_aliasing_off,
         backdrop_filter_quality, intersects_damage_under);
}

void AggregatedRenderPassDrawQuad::SetAll(
    const AggregatedRenderPassDrawQuad& other) {
  render_pass_id = other.render_pass_id;

  // DrawQuad
  DrawQuad::SetAll(other.shared_quad_state,
                   DrawQuad::Material::kAggregatedRenderPass, other.rect,
                   other.visible_rect, other.needs_blending);
  resources.ids[kMaskResourceIdIndex] =
      other.resources.ids[kMaskResourceIdIndex];
  resources.count = other.resources.count;

  // RenderPassDrawQuadInternal
  mask_uv_rect = other.mask_uv_rect;
  mask_texture_size = other.mask_texture_size;
  filters_scale = other.filters_scale;
  filters_origin = other.filters_origin;
  tex_coord_rect = other.tex_coord_rect;
  force_anti_aliasing_off = other.force_anti_aliasing_off;
  backdrop_filter_quality = other.backdrop_filter_quality;
  intersects_damage_under = other.intersects_damage_under;
}

void AggregatedRenderPassDrawQuad::SetAll(
    const SharedQuadState* shared_quad_state,
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
    bool intersects_damage_under) {
  DCHECK(render_pass);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kAggregatedRenderPass,
                   rect, visible_rect, needs_blending);
  render_pass_id = render_pass;
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
  CHECK_EQ(quad->material, DrawQuad::Material::kAggregatedRenderPass);
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
