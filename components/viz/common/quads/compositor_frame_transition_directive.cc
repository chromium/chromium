// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include <string>
#include <utility>

#include "base/time/time.h"
#include "base/trace_event/traced_value.h"

namespace viz {

namespace {
const char* TypeToString(CompositorFrameTransitionDirective::Type type) {
  switch (type) {
    case CompositorFrameTransitionDirective::Type::kSave:
      return "kSave";
    case CompositorFrameTransitionDirective::Type::kAnimateRenderer:
      return "kAnimateRenderer";
    case CompositorFrameTransitionDirective::Type::kRelease:
      return "kRelease";
  }
  NOTREACHED();
}
}  // namespace

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

void CompositorFrameTransitionDirective::SharedElement::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetInteger("render_pass_id", render_pass_id.GetUnsafeValue());
  value->SetString("view_transition_element_resource_id",
                   view_transition_element_resource_id.ToString());
}

void CompositorFrameTransitionDirective::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetString("type", TypeToString(type_));
  value->SetInteger("sequence_id", sequence_id_);
  value->SetString("transition_token", transition_token_.ToString());
  value->SetBoolean("maybe_cross_frame_sink", maybe_cross_frame_sink_);

  value->BeginDictionary("display_color_spaces");
  display_color_spaces_.AsValueInto(value);
  value->EndDictionary();

  value->BeginArray("shared_elements");
  for (const auto& element : shared_elements_) {
    value->BeginDictionary();
    element.AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();
}

}  // namespace viz
