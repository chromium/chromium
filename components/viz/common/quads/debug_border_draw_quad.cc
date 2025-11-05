// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/debug_border_draw_quad.h"

#include "base/check.h"
#include "base/trace_event/traced_value.h"
#include "ui/gfx/color_utils.h"

namespace viz {

DebugBorderDrawQuad::DebugBorderDrawQuad() = default;

void DebugBorderDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 SkColor4f c,
                                 int w) {
  bool needs_blending = c.fA < 1.0f;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kDebugBorder, rect,
                   visible_rect, needs_blending);
  color = c;
  width = w;
}

void DebugBorderDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 SkColor4f c,
                                 int w) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kDebugBorder, rect,
                   visible_rect, needs_blending);
  color = c;
  width = w;
}

const DebugBorderDrawQuad* DebugBorderDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  CHECK_EQ(quad->material, DrawQuad::Material::kDebugBorder);
  return static_cast<const DebugBorderDrawQuad*>(quad);
}

void DebugBorderDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetString("color",
                   color_utils::SkColorToRgbaString(color.toSkColor()));
  value->SetInteger("width", width);
}

}  // namespace viz
