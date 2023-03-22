// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/shared_element_draw_quad.h"

#include "base/trace_event/traced_value.h"

namespace viz {

SharedElementDrawQuad::SharedElementDrawQuad() = default;

SharedElementDrawQuad::SharedElementDrawQuad(
    const SharedElementDrawQuad& other) = default;

SharedElementDrawQuad::~SharedElementDrawQuad() = default;

SharedElementDrawQuad& SharedElementDrawQuad::operator=(
    const SharedElementDrawQuad& other) = default;

void SharedElementDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                   const gfx::Rect& rect,
                                   const gfx::Rect& visible_rect,
                                   const ViewTransitionElementResourceId& id) {
  // Force blending since at this stage we don't have information about whether
  // the replaced content will be opaque.
  bool needs_blending = true;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSharedElement, rect,
                   visible_rect, needs_blending);
  resource_id = id;
}

void SharedElementDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                   const gfx::Rect& rect,
                                   const gfx::Rect& visible_rect,
                                   bool needs_blending,
                                   const ViewTransitionElementResourceId& id) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSharedElement, rect,
                   visible_rect, needs_blending);
  resource_id = id;
}

const SharedElementDrawQuad* SharedElementDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  CHECK_EQ(quad->material, DrawQuad::Material::kSharedElement);
  return static_cast<const SharedElementDrawQuad*>(quad);
}

void SharedElementDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetString("view_transition_element_resource_id",
                   resource_id.ToString());
}

}  // namespace viz
