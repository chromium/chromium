// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler_win.h"

#include "components/input/input_router_client.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

namespace input {

using blink::WebInputEvent;

StylusHandwritingHandlerWin::StylusHandwritingHandlerWin(
    InputRouterClient* client)
    : StylusHandwritingHandler(client) {}

StylusHandwritingHandlerWin::~StylusHandwritingHandlerWin() = default;

void StylusHandwritingHandlerWin::ApplyTouchAction(
    cc::TouchAction touch_action) {
  if (!handwriting_sequence_) {
    return;
  }

  if ((touch_action & cc::TouchAction::kInternalNotWritable) ==
      cc::TouchAction::kInternalNotWritable) {
    handwriting_sequence_.reset();
    return;
  }

  handwriting_sequence_->writable_touch_action_received = true;
  TryStartStylusWriting(/*require_movement_threshold=*/true);
}

StylusHandwritingHandler::GestureHandlingResult
StylusHandwritingHandlerWin::HandleGesture(
    const blink::WebGestureEvent& event,
    std::optional<cc::TouchAction> allowed_touch_action) {
  if (event.GetType() == WebInputEvent::Type::kGestureTapDown &&
      event.primary_pointer_type == ui::EventPointerType::kPen) {
    handwriting_sequence_.emplace(event.primary_unique_touch_event_id);
    if (allowed_touch_action) {
      ApplyTouchAction(*allowed_touch_action);
    }

    // Do not consume this because it only records state for writing.
    return GestureHandlingResult::kNotHandled;
  }

  if (handwriting_sequence_ &&
      event.primary_unique_touch_event_id ==
          handwriting_sequence_->touch_start_event_id &&
      event.primary_pointer_type == ui::EventPointerType::kPen) {
    switch (event.GetType()) {
      case WebInputEvent::Type::kGestureShowPress:
        // ShowPress can be synthesized GestureTap before its timer. ShowPress
        // is used for its timer so ignore synthesized events. Synthesized
        // ShowPress can be identified by unique_touch_event_id which is
        // sourced from TouchEnd.
        if (event.unique_touch_event_id !=
            handwriting_sequence_->touch_start_event_id) {
          break;
        }
        handwriting_sequence_->show_press_seen = true;
        TryStartStylusWriting(/*require_movement_threshold=*/true);
        if (StylusWritingStarted()) {
          return GestureHandlingResult::kConsumed;
        }
        break;
      case WebInputEvent::Type::kGestureTap:
        // Support writing for under movement threshold taps that meet the
        // timing threshold for characters like punctuation.
        // Note: In the current scheme it's possible Tap doesn't write if the
        // TouchAction is late. Instead of an event queueing scheme to await
        // TouchAction, simply drop and allow for another handwriting sequence.
        TryStartStylusWriting(/*require_movement_threshold=*/false);
        handwriting_sequence_.reset();
        if (StylusWritingStarted()) {
          EndStylusWriting();
          return GestureHandlingResult::kForwardAsTapCancel;
        }
        break;
      default:
        break;
    }
  }
  return GestureHandlingResult::kNotHandled;
}

void StylusHandwritingHandlerWin::TryStartStylusWriting(
    bool require_movement_threshold) {
  if (!handwriting_sequence_ || StylusWritingStarted() ||
      (require_movement_threshold &&
       !handwriting_sequence_->movement_threshold_crossed) ||
      !handwriting_sequence_->show_press_seen ||
      !handwriting_sequence_->writable_touch_action_received) {
    return;
  }
  StartStylusWriting();
}

}  // namespace input
