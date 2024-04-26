// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/render_pass_internal.h"

#include <stddef.h>

#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/traced_value.h"

namespace viz {
namespace {
const size_t kDefaultNumSharedQuadStatesToReserve = 32;
const size_t kDefaultNumQuadsToReserve = 128;
}  // namespace

RenderPassInternal::RenderPassInternal()
    : RenderPassInternal(kDefaultNumSharedQuadStatesToReserve,
                         kDefaultNumQuadsToReserve) {}
// Each layer usually produces one shared quad state, so the number of layers
// is a good hint for what to reserve here.
RenderPassInternal::RenderPassInternal(size_t num_layers)
    : RenderPassInternal(num_layers, kDefaultNumQuadsToReserve) {}
RenderPassInternal::RenderPassInternal(size_t shared_quad_state_list_size,
                                       size_t quad_list_size)
    : quad_list(quad_list_size),
      shared_quad_state_list(alignof(SharedQuadState),
                             sizeof(SharedQuadState),
                             shared_quad_state_list_size) {}

RenderPassInternal::~RenderPassInternal() = default;

SharedQuadState* RenderPassInternal::CreateAndAppendSharedQuadState() {
  return shared_quad_state_list.AllocateAndConstruct<SharedQuadState>();
}

void RenderPassInternal::ReplaceExistingQuadWithSolidColor(
    QuadList::Iterator at,
    SkColor4f color,
    SkBlendMode blend_mode) {
  const SharedQuadState* shared_quad_state = at->shared_quad_state;
  if (shared_quad_state->are_contents_opaque ||
      shared_quad_state->blend_mode != blend_mode) {
    auto* new_shared_quad_state =
        shared_quad_state_list.AllocateAndCopyFrom(shared_quad_state);
    new_shared_quad_state->are_contents_opaque = false;
    new_shared_quad_state->blend_mode = blend_mode;
    shared_quad_state = new_shared_quad_state;
  }

  const gfx::Rect rect = at->rect;
  // TODO(crbug.com/40219248) This function should be called with an SkColor4f
  quad_list.ReplaceExistingElement<SolidColorDrawQuad>(at)->SetAll(
      shared_quad_state, rect, /*visible_rect=*/rect,
      /*needs_blending=*/false, color,
      /*force_anti_aliasing_off=*/true);
}

void RenderPassInternal::AsValueInto(
    base::trace_event::TracedValue* value) const {
  cc::MathUtil::AddToTracedValue("output_rect", output_rect, value);
  cc::MathUtil::AddToTracedValue("damage_rect", damage_rect, value);

  value->SetBoolean("has_transparent_background", has_transparent_background);
  value->SetBoolean("cache_render_pass", cache_render_pass);
  value->SetBoolean("has_damage_from_contributing_content",
                    has_damage_from_contributing_content);
  value->SetBoolean("generate_mipmap", generate_mipmap);
  value->SetInteger("copy_requests",
                    base::saturated_cast<int>(copy_requests.size()));

  value->BeginArray("filters");
  filters.AsValueInto(value);
  value->EndArray();

  value->BeginArray("backdrop_filters");
  backdrop_filters.AsValueInto(value);
  value->EndArray();

  if (backdrop_filter_bounds.has_value()) {
    cc::MathUtil::AddToTracedValue("backdrop_filter_bounds",
                                   backdrop_filter_bounds.value(), value);
  }

  value->BeginArray("shared_quad_state_list");
  for (auto* shared_quad_state : shared_quad_state_list) {
    value->BeginDictionary();
    shared_quad_state->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  value->BeginArray("quad_list");
  for (auto* quad : quad_list) {
    value->BeginDictionary();
    quad->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();
}

}  // namespace viz
