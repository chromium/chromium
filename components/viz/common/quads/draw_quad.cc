// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/draw_quad.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/quad_f.h"

namespace viz {

DrawQuad::DrawQuad()
    : material(Material::kInvalid),
      needs_blending(false),
      shared_quad_state(nullptr) {}

DrawQuad::DrawQuad(const DrawQuad& other) = default;

void DrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                      Material material,
                      const gfx::Rect& rect,
                      const gfx::Rect& visible_rect,
                      bool needs_blending) {
  DCHECK(rect.Contains(visible_rect))
      << "rect: " << rect.ToString()
      << " visible_rect: " << visible_rect.ToString();

  this->material = material;
  this->rect = rect;
  this->visible_rect = visible_rect;
  this->needs_blending = needs_blending;
  this->shared_quad_state = shared_quad_state;

  DCHECK(shared_quad_state);
  DCHECK(material != Material::kInvalid);
}

DrawQuad::~DrawQuad() {}

void DrawQuad::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("material", static_cast<int>(material));
  TracedValue::SetIDRef(shared_quad_state, value, "shared_state");

  cc::MathUtil::AddToTracedValue("content_space_rect", rect, value);

  bool rect_is_clipped;
  gfx::QuadF rect_as_target_space_quad =
      cc::MathUtil::MapQuad(shared_quad_state->quad_to_target_transform,
                            gfx::QuadF(gfx::RectF(rect)), &rect_is_clipped);
  cc::MathUtil::AddToTracedValue("rect_as_target_space_quad",
                                 rect_as_target_space_quad, value);

  value->SetBoolean("rect_is_clipped", rect_is_clipped);

  cc::MathUtil::AddToTracedValue("content_space_visible_rect", visible_rect,
                                 value);

  bool visible_rect_is_clipped;
  gfx::QuadF visible_rect_as_target_space_quad = cc::MathUtil::MapQuad(
      shared_quad_state->quad_to_target_transform,
      gfx::QuadF(gfx::RectF(visible_rect)), &visible_rect_is_clipped);

  cc::MathUtil::AddToTracedValue("visible_rect_as_target_space_quad",
                                 visible_rect_as_target_space_quad, value);

  value->SetBoolean("visible_rect_is_clipped", visible_rect_is_clipped);

  value->SetBoolean("needs_blending", needs_blending);
  value->SetBoolean("should_draw_with_blending", ShouldDrawWithBlending());
  ExtendValue(value);
}

DrawQuad::Resources::Resources() : count(0) {
  for (size_t i = 0; i < kMaxResourceIdCount; ++i)
    ids[i] = 0;
}

}  // namespace viz
