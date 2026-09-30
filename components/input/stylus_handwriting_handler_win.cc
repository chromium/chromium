// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler_win.h"

#include "base/check_op.h"
#include "components/input/input_router_client.h"
#include "components/stylus_handwriting/win/features.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace input {

using blink::WebInputEvent;

StylusHandwritingHandlerWin::HandwritingSequence::HandwritingSequence(
    const gfx::PointF& down_position_in_screen,
    uint32_t touch_start_event_id,
    const gfx::Vector2dF& pixels_per_inch,
    float device_scale_factor)
    : down_position_in_screen(down_position_in_screen),
      touch_start_event_id(touch_start_event_id),
      pixels_per_inch(pixels_per_inch),
      device_scale_factor(device_scale_factor) {
  // `ScreenWin` returns `std::nullopt` for invalid pointer-device ranges, and
  // `StylusHandwritingPropertiesWin` replaces missing PPI with a positive
  // fallback, so stored PPI must be positive.
  CHECK_GT(pixels_per_inch.x(), 0.0f);
  CHECK_GT(pixels_per_inch.y(), 0.0f);
}

StylusHandwritingHandlerWin::HandwritingSequence::~HandwritingSequence() =
    default;

StylusHandwritingHandlerWin::StylusHandwritingHandlerWin(
    InputRouterClient* client)
    : StylusHandwritingHandler(client),
      small_gesture_movement_threshold_hm_(
          stylus_handwriting::win::
              StylusHandwritingWinSmallGestureMovementThresholdHm()) {}

StylusHandwritingHandlerWin::~StylusHandwritingHandlerWin() = default;

void StylusHandwritingHandlerWin::OnTouchEvent(
    const blink::WebTouchEvent& event,
    float device_scale_factor) {
  if (event.touches_length == 0) {
    return;
  }

  const blink::WebTouchPoint& primary_touch = event.touches[0];
  if (event.IsTouchSequenceStart() && event.touches_length == 1 &&
      primary_touch.pointer_type == ui::EventPointerType::kPen) {
    handwriting_sequence_.reset();
    StylusInterface* const stylus_interface = client()->GetStylusInterface();
    if (!stylus_interface) {
      return;
    }
    const std::optional<gfx::Vector2dF> stylus_pixels_per_inch =
        stylus_interface->GetStylusHandwritingPixelsPerInch();
    if (!stylus_pixels_per_inch) {
      return;
    }
    handwriting_sequence_.emplace(primary_touch.PositionInScreen(),
                                  event.unique_touch_event_id,
                                  *stylus_pixels_per_inch, device_scale_factor);
    return;
  }

  if (!handwriting_sequence_) {
    return;
  }

  // Movement stops being relevant once handwriting has begun.
  if (StylusWritingStarted() ||
      (primary_touch.state != blink::WebTouchPoint::State::kStateMoved)) {
    return;
  }

  // Convert DIPs to HIMETRIC using the initiating pointer device's PPI.
  constexpr float kHimetricPerInch = 2540.0f;
  gfx::Vector2dF movement_in_himetric =
      primary_touch.PositionInScreen() -
      handwriting_sequence_->down_position_in_screen;
  movement_in_himetric.Scale(
      handwriting_sequence_->device_scale_factor * kHimetricPerInch /
          handwriting_sequence_->pixels_per_inch.x(),
      handwriting_sequence_->device_scale_factor * kHimetricPerInch /
          handwriting_sequence_->pixels_per_inch.y());
  if (movement_in_himetric.LengthSquared() >
      small_gesture_movement_threshold_hm_ *
          small_gesture_movement_threshold_hm_) {
    handwriting_sequence_->movement_threshold_crossed = true;
  }
  TryStartStylusWriting(/*require_movement_threshold=*/true);
}

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
    if (!handwriting_sequence_ || handwriting_sequence_->touch_start_event_id !=
                                      event.primary_unique_touch_event_id) {
      handwriting_sequence_.reset();
      return GestureHandlingResult::kNotHandled;
    }

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
