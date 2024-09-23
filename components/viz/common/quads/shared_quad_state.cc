// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/shared_quad_state.h"

#include <optional>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace viz {

SharedQuadState::SharedQuadState() = default;

SharedQuadState::SharedQuadState(const SharedQuadState& other) = default;
SharedQuadState& SharedQuadState::operator=(const SharedQuadState& other) =
    default;

SharedQuadState::~SharedQuadState() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                     "viz::SharedQuadState", this);
}

bool SharedQuadState::Equals(const SharedQuadState& other) const {
  // Skip |overlay_damage_index| and |is_fast_rounded_corner|, which are added
  // in SurfaceAggregator. They don't really control the rendering effect.
  return quad_to_target_transform == other.quad_to_target_transform &&
         quad_layer_rect == other.quad_layer_rect &&
         visible_quad_layer_rect == other.visible_quad_layer_rect &&
         mask_filter_info == other.mask_filter_info &&
         clip_rect == other.clip_rect &&
         are_contents_opaque == other.are_contents_opaque &&
         opacity == other.opacity && blend_mode == other.blend_mode &&
         sorting_context_id == other.sorting_context_id &&
         layer_id == other.layer_id &&
         layer_namespace_id == other.layer_namespace_id &&
         offset_tag == other.offset_tag;
}

void SharedQuadState::SetAll(const SharedQuadState& other) {
  quad_to_target_transform = other.quad_to_target_transform;
  quad_layer_rect = other.quad_layer_rect;
  visible_quad_layer_rect = other.visible_quad_layer_rect;
  mask_filter_info = other.mask_filter_info;
  clip_rect = other.clip_rect;
  are_contents_opaque = other.are_contents_opaque;
  opacity = other.opacity;
  blend_mode = other.blend_mode;
  sorting_context_id = other.sorting_context_id;
  layer_id = other.layer_id;
  layer_namespace_id = other.layer_namespace_id;
  is_fast_rounded_corner = other.is_fast_rounded_corner;
  offset_tag = other.offset_tag;
}

void SharedQuadState::SetAll(const gfx::Transform& transform,
                             const gfx::Rect& layer_rect,
                             const gfx::Rect& visible_layer_rect,
                             const gfx::MaskFilterInfo& filter_info,
                             const std::optional<gfx::Rect>& clip,
                             bool contents_opaque,
                             float opacity_f,
                             SkBlendMode blend,
                             int sorting_context,
                             uint32_t layer,
                             bool fast_rounded_corner) {
  quad_to_target_transform = transform;
  quad_layer_rect = layer_rect;
  visible_quad_layer_rect = visible_layer_rect;
  mask_filter_info = filter_info;
  clip_rect = clip;
  are_contents_opaque = contents_opaque;
  opacity = opacity_f;
  blend_mode = blend;
  sorting_context_id = sorting_context;
  layer_id = layer;
  is_fast_rounded_corner = fast_rounded_corner;
}

void SharedQuadState::AsValueInto(base::trace_event::TracedValue* value) const {
  cc::MathUtil::AddToTracedValue("transform", quad_to_target_transform, value);
  cc::MathUtil::AddToTracedValue("layer_content_rect", quad_layer_rect, value);
  cc::MathUtil::AddToTracedValue("layer_visible_content_rect",
                                 visible_quad_layer_rect, value);
  cc::MathUtil::AddToTracedValue("mask_filter_bounds",
                                 mask_filter_info.bounds(), value);
  if (mask_filter_info.HasRoundedCorners()) {
    cc::MathUtil::AddCornerRadiiToTracedValue(
        "mask_filter_rounded_corners_radii",
        mask_filter_info.rounded_corner_bounds(), value);
  }
  if (mask_filter_info.HasGradientMask()) {
    cc::MathUtil::AddToTracedValue("mask_filter_gradient_mask",
                                   mask_filter_info.gradient_mask().value(),
                                   value);
  }

  if (clip_rect) {
    cc::MathUtil::AddToTracedValue("clip_rect", *clip_rect, value);
  }

  value->SetBoolean("are_contents_opaque", are_contents_opaque);
  value->SetDouble("opacity", opacity);
  value->SetString("blend_mode", SkBlendMode_Name(blend_mode));
  value->SetInteger("sorting_context_id", sorting_context_id);
  value->SetInteger("layer_id", layer_id);
  value->SetInteger("layer_namespace_id", layer_id);
  value->SetBoolean("is_fast_rounded_corner", is_fast_rounded_corner);
  if (offset_tag) {
    value->SetString("offset_tag", offset_tag.ToString());
  }

  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("viz.quads"), value, "viz::SharedQuadState",
      this);
}

}  // namespace viz
