// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/debug_border_draw_quad.h"

#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"

namespace viz {

DebugBorderDrawQuad::DebugBorderDrawQuad() = default;

void DebugBorderDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 SkColor color,
                                 int width) {
  bool needs_blending = SkColorGetA(color) < 255;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kDebugBorder, rect,
                   visible_rect, needs_blending);
  this->color = color;
  this->width = width;
}

void DebugBorderDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 SkColor color,
                                 int width) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kDebugBorder, rect,
                   visible_rect, needs_blending);
  this->color = color;
  this->width = width;
}

const DebugBorderDrawQuad* DebugBorderDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::Material::kDebugBorder);
  return static_cast<const DebugBorderDrawQuad*>(quad);
}

void DebugBorderDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetInteger("color", color);
  value->SetInteger("width", width);
}

}  // namespace viz
