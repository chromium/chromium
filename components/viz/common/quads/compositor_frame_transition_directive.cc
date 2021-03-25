// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include <utility>

#include "base/time/time.h"

namespace viz {

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    uint32_t sequence_id,
    Type type,
    Effect effect,
    std::vector<CompositorRenderPassId> shared_render_pass_ids)
    : sequence_id_(sequence_id),
      type_(type),
      effect_(effect),
      shared_render_pass_ids_(std::move(shared_render_pass_ids)) {}

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    const CompositorFrameTransitionDirective&) = default;

CompositorFrameTransitionDirective::~CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective&
CompositorFrameTransitionDirective::operator=(
    const CompositorFrameTransitionDirective&) = default;

}  // namespace viz
