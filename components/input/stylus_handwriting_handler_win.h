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

namespace input {

// Tracks Windows stylus gestures that may initiate handwriting before they
// become scroll gestures. A pen tap can write after its ShowPress timer has
// elapsed, supporting characters with little movement such as punctuation.
class COMPONENT_EXPORT(INPUT) StylusHandwritingHandlerWin
    : public StylusHandwritingHandler {
 public:
  explicit StylusHandwritingHandlerWin(InputRouterClient* client);

  StylusHandwritingHandlerWin(const StylusHandwritingHandlerWin&) = delete;
  StylusHandwritingHandlerWin& operator=(const StylusHandwritingHandlerWin&) =
      delete;

  ~StylusHandwritingHandlerWin() override;
  void ApplyTouchAction(cc::TouchAction touch_action) override;
  GestureHandlingResult HandleGesture(
      const blink::WebGestureEvent& event,
      std::optional<cc::TouchAction> allowed_touch_action) override;

  bool has_handwriting_sequence_for_testing() const {
    return handwriting_sequence_.has_value();
  }

 private:
  struct HandwritingSequence {
    explicit HandwritingSequence(uint32_t touch_start_event_id)
        : touch_start_event_id(touch_start_event_id) {}

    uint32_t touch_start_event_id;
    bool show_press_seen = false;
    bool movement_threshold_crossed = false;
    bool writable_touch_action_received = false;
  };

  void TryStartStylusWriting(bool require_movement_threshold);

  std::optional<HandwritingSequence> handwriting_sequence_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_WIN_H_
