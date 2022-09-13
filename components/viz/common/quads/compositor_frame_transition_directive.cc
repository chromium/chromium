// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include <string>
#include <utility>

#include "base/time/time.h"

namespace viz {

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    uint32_t sequence_id,
    Type type,
    Effect effect,
    std::vector<SharedElement> shared_elements)
    : sequence_id_(sequence_id),
      type_(type),
      effect_(effect),
      shared_elements_(std::move(shared_elements)) {}

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    const CompositorFrameTransitionDirective&) = default;

CompositorFrameTransitionDirective::~CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective&
CompositorFrameTransitionDirective::operator=(
    const CompositorFrameTransitionDirective&) = default;

CompositorFrameTransitionDirective::SharedElement::SharedElement() = default;
CompositorFrameTransitionDirective::SharedElement::~SharedElement() = default;

CompositorFrameTransitionDirective::SharedElement::SharedElement(
    const SharedElement&) = default;
CompositorFrameTransitionDirective::SharedElement&
CompositorFrameTransitionDirective::SharedElement::operator=(
    const SharedElement&) = default;

CompositorFrameTransitionDirective::SharedElement::SharedElement(
    SharedElement&&) = default;
CompositorFrameTransitionDirective::SharedElement&
CompositorFrameTransitionDirective::SharedElement::operator=(SharedElement&&) =
    default;

}  // namespace viz
