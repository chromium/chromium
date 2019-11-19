// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/content_draw_quad_base.h"

#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"

namespace viz {

ContentDrawQuadBase::ContentDrawQuadBase() = default;

ContentDrawQuadBase::~ContentDrawQuadBase() = default;

void ContentDrawQuadBase::SetNew(const SharedQuadState* shared_quad_state,
                                 DrawQuad::Material material,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 const gfx::RectF& tex_coord_rect,
                                 const gfx::Size& texture_size,
                                 bool is_premultiplied,
                                 bool nearest_neighbor,
                                 bool force_anti_aliasing_off) {
  DrawQuad::SetAll(shared_quad_state, material, rect, visible_rect,
                   needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->is_premultiplied = is_premultiplied;
  this->nearest_neighbor = nearest_neighbor;
  this->force_anti_aliasing_off = force_anti_aliasing_off;
}

void ContentDrawQuadBase::SetAll(const SharedQuadState* shared_quad_state,
                                 DrawQuad::Material material,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 const gfx::RectF& tex_coord_rect,
                                 const gfx::Size& texture_size,
                                 bool is_premultiplied,
                                 bool nearest_neighbor,
                                 bool force_anti_aliasing_off) {
  DrawQuad::SetAll(shared_quad_state, material, rect, visible_rect,
                   needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->is_premultiplied = is_premultiplied;
  this->nearest_neighbor = nearest_neighbor;
  this->force_anti_aliasing_off = force_anti_aliasing_off;
}

void ContentDrawQuadBase::ExtendValue(
    base::trace_event::TracedValue* value) const {
  cc::MathUtil::AddToTracedValue("tex_coord_rect", tex_coord_rect, value);
  cc::MathUtil::AddToTracedValue("texture_size", texture_size, value);

  value->SetBoolean("nearest_neighbor", nearest_neighbor);
  value->SetBoolean("force_anti_aliasing_off", force_anti_aliasing_off);
}

}  // namespace viz
