// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/render_pass_draw_quad.h"

#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace viz {

RenderPassDrawQuad::RenderPassDrawQuad() = default;

RenderPassDrawQuad::RenderPassDrawQuad(const RenderPassDrawQuad& other) =
    default;

RenderPassDrawQuad::~RenderPassDrawQuad() = default;

void RenderPassDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                RenderPassId render_pass_id,
                                ResourceId mask_resource_id,
                                const gfx::RectF& mask_uv_rect,
                                const gfx::Size& mask_texture_size,
                                const gfx::Vector2dF& filters_scale,
                                const gfx::PointF& filters_origin,
                                const gfx::RectF& tex_coord_rect,
                                bool force_anti_aliasing_off) {
  DCHECK(render_pass_id);

  bool needs_blending = true;
  SetAll(shared_quad_state, rect, visible_rect, needs_blending, render_pass_id,
         mask_resource_id, mask_uv_rect, mask_texture_size, filters_scale,
         filters_origin, tex_coord_rect, force_anti_aliasing_off);
}

void RenderPassDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
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
                                bool force_anti_aliasing_off) {
  DCHECK(render_pass_id);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::RENDER_PASS, rect, visible_rect,
                   needs_blending);
  this->render_pass_id = render_pass_id;
  resources.ids[kMaskResourceIdIndex] = mask_resource_id;
  resources.count = mask_resource_id ? 1 : 0;
  this->mask_uv_rect = mask_uv_rect;
  this->mask_texture_size = mask_texture_size;
  this->filters_scale = filters_scale;
  this->filters_origin = filters_origin;
  this->tex_coord_rect = tex_coord_rect;
  this->force_anti_aliasing_off = force_anti_aliasing_off;
}

const RenderPassDrawQuad* RenderPassDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::RENDER_PASS);
  return static_cast<const RenderPassDrawQuad*>(quad);
}

void RenderPassDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  TracedValue::SetIDRef(reinterpret_cast<void*>(render_pass_id), value,
                        "render_pass_id");
  value->SetInteger("mask_resource_id", resources.ids[kMaskResourceIdIndex]);
  cc::MathUtil::AddToTracedValue("mask_texture_size", mask_texture_size, value);
  cc::MathUtil::AddToTracedValue("mask_uv_rect", mask_uv_rect, value);
  cc::MathUtil::AddToTracedValue("tex_coord_rect", tex_coord_rect, value);
  value->SetBoolean("force_anti_aliasing_off", force_anti_aliasing_off);
}

}  // namespace viz
