// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include <string>
#include <utility>

#include "base/time/time.h"

namespace viz {
namespace {

constexpr base::TimeDelta kDefaultTransitionDuration = base::Milliseconds(250);
constexpr base::TimeDelta kDefaultTransitionDelay = base::Milliseconds(0);

}  // namespace

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    uint32_t sequence_id,
    Type type,
    bool is_renderer_driven_animation,
    Effect effect,
    const TransitionConfig& root_config,
    std::vector<SharedElement> shared_elements)
    : sequence_id_(sequence_id),
      type_(type),
      is_renderer_driven_animation_(is_renderer_driven_animation),
      effect_(effect),
      root_config_(root_config),
      shared_elements_(std::move(shared_elements)) {}

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    const CompositorFrameTransitionDirective&) = default;

CompositorFrameTransitionDirective::~CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective&
CompositorFrameTransitionDirective::operator=(
    const CompositorFrameTransitionDirective&) = default;

CompositorFrameTransitionDirective::TransitionConfig::TransitionConfig()
    : duration(kDefaultTransitionDuration), delay(kDefaultTransitionDelay) {}

bool CompositorFrameTransitionDirective::TransitionConfig::IsValid(
    std::string* error) const {
  constexpr base::TimeDelta kMinValue = base::Seconds(0);
  constexpr base::TimeDelta kMaxValue = base::Seconds(5);

  if (duration < kMinValue || duration > kMaxValue) {
    if (error)
      *error = "Duration must be between 0 and 5 seconds (inclusive)";
    return false;
  }

  if (delay < kMinValue || delay > kMaxValue) {
    if (error)
      *error = "Delay must be between 0 and 5 seconds (inclusive)";
    return false;
  }

  return true;
}

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
