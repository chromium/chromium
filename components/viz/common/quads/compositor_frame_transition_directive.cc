// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include <string>
#include <utility>

#include "base/time/time.h"

namespace viz {

// static
CompositorFrameTransitionDirective
CompositorFrameTransitionDirective::CreateSave(
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink,
    uint32_t sequence_id,
    std::vector<SharedElement> shared_elements,
    const gfx::DisplayColorSpaces& display_color_spaces) {
  return CompositorFrameTransitionDirective(
      transition_token, maybe_cross_frame_sink, sequence_id, Type::kSave,
      std::move(shared_elements), display_color_spaces);
}

// static
CompositorFrameTransitionDirective
CompositorFrameTransitionDirective::CreateAnimate(
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink,
    uint32_t sequence_id) {
  return CompositorFrameTransitionDirective(transition_token,
                                            maybe_cross_frame_sink, sequence_id,
                                            Type::kAnimateRenderer);
}

// static
CompositorFrameTransitionDirective
CompositorFrameTransitionDirective::CreateRelease(
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink,
    uint32_t sequence_id) {
  return CompositorFrameTransitionDirective(
      transition_token, maybe_cross_frame_sink, sequence_id, Type::kRelease);
}

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink,
    uint32_t sequence_id,
    Type type,
    std::vector<SharedElement> shared_elements,
    const gfx::DisplayColorSpaces& display_color_spaces)
    : transition_token_(transition_token),
      maybe_cross_frame_sink_(maybe_cross_frame_sink),
      sequence_id_(sequence_id),
      type_(type),
      shared_elements_(std::move(shared_elements)),
      display_color_spaces_(display_color_spaces) {}

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
