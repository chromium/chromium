// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/solid_color_draw_quad.h"

#include <algorithm>

#include "base/check.h"
#include "base/trace_event/traced_value.h"
#include "ui/gfx/color_utils.h"

namespace viz {

SolidColorDrawQuad::SolidColorDrawQuad()
    : color(SkColors::kTransparent), force_anti_aliasing_off(false) {}

void SolidColorDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                SkColor4f c,
                                bool anti_aliasing_off) {
  // Clamp the alpha component of the color to the range of [0, 1].
  c.fA = std::clamp(c.fA, 0.0f, 1.0f);

  bool needs_blending = !c.isOpaque();
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSolidColor, rect,
                   visible_rect, needs_blending);
  color = c;
  force_anti_aliasing_off = anti_aliasing_off;
}

void SolidColorDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                bool needs_blending,
                                SkColor4f c,
                                bool anti_aliasing_off) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSolidColor, rect,
                   visible_rect, needs_blending);
  color = c;
  force_anti_aliasing_off = anti_aliasing_off;
}

const SolidColorDrawQuad* SolidColorDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  CHECK_EQ(quad->material, DrawQuad::Material::kSolidColor);
  return static_cast<const SolidColorDrawQuad*>(quad);
}

void SolidColorDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetString("color", color_utils::SkColor4fToRgbaString(color));
  value->SetBoolean("force_anti_aliasing_off", force_anti_aliasing_off);
}

}  // namespace viz
