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
    NavigationID navigation_id,
    uint32_t sequence_id,
    Type type,
    std::vector<SharedElement> shared_elements)
    : navigation_id_(navigation_id),
      sequence_id_(sequence_id),
      type_(type),
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

bool CompositorFrameTransitionDirective::SharedElement::operator==(
    const SharedElement& other) const {
  return render_pass_id == other.render_pass_id &&
         view_transition_element_resource_id ==
             other.view_transition_element_resource_id;
}

bool CompositorFrameTransitionDirective::SharedElement::operator!=(
    const SharedElement& other) const {
  return !(other == *this);
}

}  // namespace viz
