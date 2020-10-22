// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/render_pass_internal.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

#include <stddef.h>

#include "components/viz/common/frame_sinks/copy_output_request.h"

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

void RenderPassInternal::ReplaceExistingQuadWithOpaqueTransparentSolidColor(
    QuadList::Iterator at) {
  // In order to fill the backbuffer with transparent black, the replacement
  // solid color quad needs to set |needs_blending| to false, and
  // ShouldDrawWithBlending() returns false so it is drawn without blending.
  const gfx::Rect rect = at->rect;
  bool needs_blending = false;
  const SharedQuadState* shared_quad_state = at->shared_quad_state;
  if (shared_quad_state->are_contents_opaque) {
    auto* new_shared_quad_state =
        shared_quad_state_list.AllocateAndCopyFrom(shared_quad_state);
    new_shared_quad_state->are_contents_opaque = false;
    shared_quad_state = new_shared_quad_state;
  }

  auto* replacement = quad_list.ReplaceExistingElement<SolidColorDrawQuad>(at);
  replacement->SetAll(shared_quad_state, rect, rect /* visible_rect */,
                      needs_blending, SK_ColorTRANSPARENT, true);
}

}  // namespace viz
