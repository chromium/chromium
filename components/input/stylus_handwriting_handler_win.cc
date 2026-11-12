// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler_win.h"

#include "base/check.h"
#include "base/check_op.h"
#include "components/input/input_router_client.h"
#include "components/stylus_handwriting/win/features.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace input {

using blink::WebInputEvent;

StylusHandwritingHandlerWin::HandwritingSequence::HandwritingSequence(
    const blink::WebTouchEvent& event,
    const ui::StylusHandwritingPropertiesWin& properties,
    float device_scale_factor)
    : down_position_in_screen(event.touches[0].PositionInScreen()),
      touch_start_event_id(event.unique_touch_event_id),
      pointer_id(event.touches[0].id),
      properties(properties),
      device_scale_factor(device_scale_factor) {
  // `ScreenWin` returns `std::nullopt` for invalid pointer-device ranges, and
  // `StylusHandwritingPropertiesWin` replaces missing PPI with a positive
  // fallback, so stored PPI must be positive.
  CHECK_GT(properties.pixels_per_inch.x(), 0.0f);
  CHECK_GT(properties.pixels_per_inch.y(), 0.0f);
}

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
  if (event.IsTouchSequenceStart()) {
    if (event.touches_length != 1 ||
        primary_touch.pointer_type != ui::EventPointerType::kPen) {
      return;
    }
    StylusInterface* const stylus_interface = client()->GetStylusInterface();
    if (!stylus_interface) {
      return;
    }
    const std::optional<ui::StylusHandwritingPropertiesWin> properties =
        stylus_interface->TakeStylusHandwritingProperties(
            event.unique_touch_event_id);
    if (!properties) {
      return;
    }
    handwriting_sequences_.emplace_back(event, *properties,
                                        device_scale_factor);
    return;
  }

  // Once the pen is released, it may have been removed from the touches array.
  // Do not process further touch events.
  if (handwriting_sequences_.empty() ||
      handwriting_sequences_.back().primary_release_event_id) {
    return;
  }

  // Sequences are queued per touch sequence start. Touch sequence starts
  // occur for first single pointer contact. So it is guaranteed that any
  // subsequent raw input events are for the most recently queued sequence.
  HandwritingSequence& sequence = handwriting_sequences_.back();
  const bool single_queued_sequence = handwriting_sequences_.size() == 1;

  // TODO(crbug.com/355578906): Track the initiating pen independently of the
  // touch sequence so additional contacts do not cancel handwriting
  // eligibility while that pen remains active.
  if (!(single_queued_sequence && StylusWritingStarted()) &&
      event.touches_length > 1) {
    sequence.eligible = false;
  }

  const blink::WebTouchPoint* pen = nullptr;
  for (unsigned i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].id == sequence.pointer_id &&
        event.touches[i].pointer_type == ui::EventPointerType::kPen) {
      pen = &event.touches[i];
      break;
    }
  }
  if (!pen) {
    return;
  }

  // GestureEnd is emitted for every pointer in the gesture. Track the primary
  // pointer's release event id so the matching GestureEnd resets the sequence.
  if (pen->state == blink::WebTouchPoint::State::kStateReleased ||
      pen->state == blink::WebTouchPoint::State::kStateCancelled) {
    sequence.primary_release_event_id = event.unique_touch_event_id;
    return;
  }

  // Movement stops being relevant once the first sequence has started writing
  // or has been disqualified. A newer queued sequence still tracks movement
  // while an older sequence is writing.
  if (!sequence.eligible ||
      (single_queued_sequence && StylusWritingStarted()) ||
      (pen->state != blink::WebTouchPoint::State::kStateMoved)) {
    return;
  }

  // Convert DIPs to HIMETRIC using the initiating pointer device's PPI.
  constexpr float kHimetricPerInch = 2540.0f;
  gfx::Vector2dF movement_in_himetric =
      pen->PositionInScreen() - sequence.down_position_in_screen;
  movement_in_himetric.Scale(sequence.device_scale_factor * kHimetricPerInch /
                                 sequence.properties.pixels_per_inch.x(),
                             sequence.device_scale_factor * kHimetricPerInch /
                                 sequence.properties.pixels_per_inch.y());
  if (movement_in_himetric.LengthSquared() >
      small_gesture_movement_threshold_hm_ *
          small_gesture_movement_threshold_hm_) {
    sequence.movement_threshold_crossed = true;
  }

  // A newer sequence's GestureEvents are guaranteed to arrive after those of
  // an older one. So until a sequence reaches the front of the queue, it can
  // only latch the movement threshold here. It attempts writing when its
  // GestureEvents are handled.
  if (single_queued_sequence) {
    TryStartStylusWriting(/*require_movement_threshold=*/true);
  }
}

void StylusHandwritingHandlerWin::ApplyTouchAction(
    cc::TouchAction touch_action) {
  // This might be called in response to an additional contact after the
  // primary contact has started writing.
  if (handwriting_sequences_.empty() || StylusWritingStarted()) {
    return;
  }

  if ((touch_action & cc::TouchAction::kInternalNotWritable) ==
      cc::TouchAction::kInternalNotWritable) {
    handwriting_sequences_.front().eligible = false;
    return;
  }

  handwriting_sequences_.front().writable_touch_action_received = true;
  TryStartStylusWriting(/*require_movement_threshold=*/true);
}

StylusHandwritingHandler::GestureHandlingResult
StylusHandwritingHandlerWin::HandleGesture(
    const blink::WebGestureEvent& event,
    std::optional<cc::TouchAction> allowed_touch_action) {
  // GestureBegin is per-contact and can carry a newer ID within this sequence.
  if (!handwriting_sequences_.empty() &&
      event.SourceDevice() == blink::WebGestureDevice::kTouchscreen &&
      event.primary_pointer_type == ui::EventPointerType::kPen &&
      event.GetType() != WebInputEvent::Type::kGestureBegin &&
      event.primary_unique_touch_event_id) {
    // We rely on kEnableGestureBeginEndTypes to forward GestureEnd even for a
    // consumed TouchStart, so the front retires before a newer sequence's
    // gestures arrive. We shouldn't be able to receive a gesture for a newer
    // sequence before the front retires, so DCHECK in case feature enablement
    // changes for Windows.
    DCHECK_LE(event.primary_unique_touch_event_id,
              handwriting_sequences_.front().touch_start_event_id);
    while (!handwriting_sequences_.empty() &&
           handwriting_sequences_.front().touch_start_event_id <
               event.primary_unique_touch_event_id) {
      EndStylusWriting();
    }
  }

  if (!IsGestureForCurrentSequence(event)) {
    return GestureHandlingResult::kNotHandled;
  }

  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureTapDown:
      handwriting_sequences_.front().needs_tap_end = true;
      if (allowed_touch_action) {
        ApplyTouchAction(*allowed_touch_action);
      }
      break;
    case WebInputEvent::Type::kGestureScrollBegin:
      handwriting_sequences_.front().is_scrolling = true;
      if (allowed_touch_action) {
        ApplyTouchAction(*allowed_touch_action);
      }
      break;
    case WebInputEvent::Type::kGestureTapCancel:
      if (handwriting_sequences_.front().tap_cancelled) {
        return GestureHandlingResult::kConsumed;
      }
      handwriting_sequences_.front().tap_cancelled = true;
      handwriting_sequences_.front().needs_tap_end = false;
      break;
    case WebInputEvent::Type::kGestureShowPress: {
      // If GestureTap arrives before the ShowPress timer fires, the gesture
      // filter synthesizes ShowPress ahead of it. Only the timer-driven
      // ShowPress indicates sufficient dwell, so ignore synthesized ones.
      // Synthesized ShowPress is identified by its unique_touch_event_id,
      // which is sourced from TouchEnd rather than TouchStart.
      if (event.unique_touch_event_id !=
          handwriting_sequences_.front().touch_start_event_id) {
        break;
      }
      handwriting_sequences_.front().show_press_seen = true;
      const bool was_writing = StylusWritingStarted();

      // If we are writing at this point, then it must have been by scrolling.
      // Because ShowPress is required for small gesture writing.
      CHECK(!was_writing || handwriting_sequences_.front().is_scrolling);

      if (!TryStartStylusWriting(/*require_movement_threshold=*/true) &&
          !was_writing) {
        break;
      }
    }
      [[fallthrough]];
    case WebInputEvent::Type::kGestureShortPress:
    case WebInputEvent::Type::kGestureLongPress:
      if (StylusWritingStarted()) {
        // Use the next press to end the tap without injecting a separate event
        // when raw input or touch action starts writing.
        if (handwriting_sequences_.front().needs_tap_end) {
          handwriting_sequences_.front().needs_tap_end = false;
          handwriting_sequences_.front().tap_cancelled = true;
          return GestureHandlingResult::kForwardAsTapCancel;
        }
        return GestureHandlingResult::kConsumed;
      }
      // Preserve drag-and-drop and context-menu behavior if writing never
      // began.
      handwriting_sequences_.front().eligible = false;
      break;

    // Terminal gesture handling.
    case WebInputEvent::Type::kGestureTap: {
      // Support writing for under movement threshold taps that meet the
      // timing threshold for characters like punctuation.
      const bool tap_cancelled = handwriting_sequences_.front().tap_cancelled;
      const bool was_writing = StylusWritingStarted();
      handwriting_sequences_.front().needs_tap_end = false;
      const bool started =
          TryStartStylusWriting(/*require_movement_threshold=*/false);
      EndStylusWriting();
      if (started || was_writing) {
        return tap_cancelled ? GestureHandlingResult::kConsumed
                             : GestureHandlingResult::kForwardAsTapCancel;
      }
      break;
    }
    case WebInputEvent::Type::kGestureDoubleTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
    case WebInputEvent::Type::kGestureLongTap:
      if (StylusWritingStarted()) {
        EndStylusWriting();
        return GestureHandlingResult::kConsumed;
      }
      handwriting_sequences_.pop_front();
      break;
    case WebInputEvent::Type::kGestureEnd:
      if (!handwriting_sequences_.front().primary_release_event_id ||
          event.unique_touch_event_id !=
              *handwriting_sequences_.front().primary_release_event_id) {
        break;
      }
      // Another contact can keep the scroll alive after the pen is lifted.
      if (!handwriting_sequences_.front().is_scrolling ||
          !StylusWritingStarted()) {
        EndStylusWriting();
      }
      break;
    default:
      break;
  }
  return GestureHandlingResult::kNotHandled;
}

bool StylusHandwritingHandlerWin::StartStylusWriting() {
  // The router's scroll-writing path calls Start directly, bypassing Try's
  // small-gesture checks, so sequence eligibility must also be checked here.
  if (handwriting_sequences_.empty() ||
      !handwriting_sequences_.front().eligible) {
    return false;
  }
  return StylusWritingStarted() ||
         StylusHandwritingHandler::StartStylusWriting();
}

void StylusHandwritingHandlerWin::OnStartStylusWriting() {
  CHECK(!handwriting_sequences_.empty());
  const ui::StylusHandwritingPropertiesWin properties =
      handwriting_sequences_.front().properties;
  client()->OnStartStylusWriting(properties);
}

bool StylusHandwritingHandlerWin::IsGestureForCurrentSequence(
    const blink::WebGestureEvent& event) const {
  if (handwriting_sequences_.empty() ||
      event.SourceDevice() != blink::WebGestureDevice::kTouchscreen ||
      event.primary_pointer_type != ui::EventPointerType::kPen) {
    return false;
  }
  const HandwritingSequence& sequence = handwriting_sequences_.front();
  if (!event.primary_unique_touch_event_id) {
    // The gesture filter synthesizes TapCancel without the initiating touch ID.
    // A pen tap is always closed before its sequence retires, so an ID-less
    // pen TapCancel belongs to the front sequence.
    return event.GetType() == WebInputEvent::Type::kGestureTapCancel;
  }
  return event.primary_unique_touch_event_id == sequence.touch_start_event_id;
}

bool StylusHandwritingHandlerWin::TryStartStylusWriting(
    bool require_movement_threshold) {
  if (handwriting_sequences_.empty() || StylusWritingStarted()) {
    return false;
  }
  const HandwritingSequence& sequence = handwriting_sequences_.front();
  if ((require_movement_threshold && !sequence.movement_threshold_crossed) ||
      !sequence.show_press_seen || !sequence.writable_touch_action_received) {
    return false;
  }
  return StartStylusWriting();
}

void StylusHandwritingHandlerWin::EndStylusWriting() {
  if (!handwriting_sequences_.empty()) {
    handwriting_sequences_.pop_front();
  }
  StylusHandwritingHandler::EndStylusWriting();
}

}  // namespace input
