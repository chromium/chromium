// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_WIN_H_
#define COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_WIN_H_

#include <cstdint>
#include <optional>

#include "base/component_export.h"
#include "cc/input/touch_action.h"
#include "components/input/stylus_handwriting_handler.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace input {

// Owns the Windows-specific stylus handwriting state for an InputRouterImpl.
// Windows supports handwriting smaller than the threshold for
// GestureScrollBegin. Specifically these scenarios:
//   1. Gesture must dwell long enough for GestureShowPress AND
//   2. Gesture must either:
//     a. Move enough to cross the SmallGestureMovementThreshold, or
//     b. Resolve as GestureTap - this is to support characters with low
//        movement thresholds like punctuation.
// Movements that are faster than GestureShowPress and large enough to
// generate GestureScrollBegin will exercise the scroll handling handwriting
// path in InputRouterImpl.
// GestureTap resets handwriting state.
// A lower movement threshold for handwriting aligns the Windows experience
// more closely with the default Shell Handwriting experience.
class COMPONENT_EXPORT(INPUT) StylusHandwritingHandlerWin
    : public StylusHandwritingHandler {
 public:
  explicit StylusHandwritingHandlerWin(InputRouterClient* client);

  StylusHandwritingHandlerWin(const StylusHandwritingHandlerWin&) = delete;
  StylusHandwritingHandlerWin& operator=(const StylusHandwritingHandlerWin&) =
      delete;

  ~StylusHandwritingHandlerWin() override;
  void OnTouchEvent(const blink::WebTouchEvent& event,
                    float device_scale_factor) override;
  void ApplyTouchAction(cc::TouchAction touch_action) override;
  GestureHandlingResult HandleGesture(
      const blink::WebGestureEvent& event,
      std::optional<cc::TouchAction> allowed_touch_action) override;

  bool has_handwriting_sequence_for_testing() const {
    return handwriting_sequence_.has_value();
  }

 private:
  struct HandwritingSequence {
    HandwritingSequence(const gfx::PointF& down_position_in_screen,
                        uint32_t touch_start_event_id,
                        const gfx::Vector2dF& pixels_per_inch,
                        float device_scale_factor);
    ~HandwritingSequence();

    gfx::PointF down_position_in_screen;
    uint32_t touch_start_event_id;
    gfx::Vector2dF pixels_per_inch;
    float device_scale_factor;
    bool show_press_seen = false;
    bool movement_threshold_crossed = false;
    bool writable_touch_action_received = false;
  };

  void TryStartStylusWriting(bool require_movement_threshold);

  const float small_gesture_movement_threshold_hm_;
  std::optional<HandwritingSequence> handwriting_sequence_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_WIN_H_
