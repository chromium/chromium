// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/shared_quad_state.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace viz {

SharedQuadState::SharedQuadState() = default;
SharedQuadState::SharedQuadState(const SharedQuadState& other) = default;
SharedQuadState::~SharedQuadState() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                     "viz::SharedQuadState", this);
}

void SharedQuadState::SetAll(const gfx::Transform& quad_to_target_transform,
                             const gfx::Rect& quad_layer_rect,
                             const gfx::Rect& visible_quad_layer_rect,
                             const gfx::RRectF& rounded_corner_bounds,
                             const gfx::Rect& clip_rect,
                             bool is_clipped,
                             bool are_contents_opaque,
                             float opacity,
                             SkBlendMode blend_mode,
                             int sorting_context_id) {
  this->quad_to_target_transform = quad_to_target_transform;
  this->quad_layer_rect = quad_layer_rect;
  this->visible_quad_layer_rect = visible_quad_layer_rect;
  this->rounded_corner_bounds = rounded_corner_bounds;
  this->clip_rect = clip_rect;
  this->is_clipped = is_clipped;
  this->are_contents_opaque = are_contents_opaque;
  this->opacity = opacity;
  this->blend_mode = blend_mode;
  this->sorting_context_id = sorting_context_id;
}

void SharedQuadState::AsValueInto(base::trace_event::TracedValue* value) const {
  cc::MathUtil::AddToTracedValue("transform", quad_to_target_transform, value);
  cc::MathUtil::AddToTracedValue("layer_content_rect", quad_layer_rect, value);
  cc::MathUtil::AddToTracedValue("layer_visible_content_rect",
                                 visible_quad_layer_rect, value);
  cc::MathUtil::AddToTracedValue("rounded_corner_bounds", rounded_corner_bounds,
                                 value);

  value->SetBoolean("is_clipped", is_clipped);
  cc::MathUtil::AddToTracedValue("clip_rect", clip_rect, value);

  value->SetBoolean("are_contents_opaque", are_contents_opaque);
  value->SetDouble("opacity", opacity);
  value->SetString("blend_mode", SkBlendMode_Name(blend_mode));
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("viz.quads"), value, "viz::SharedQuadState",
      this);
}

}  // namespace viz
