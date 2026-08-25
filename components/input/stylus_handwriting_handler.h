// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_H_
#define COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "cc/input/touch_action.h"

namespace blink {
class WebGestureEvent;
class WebTouchEvent;
}  // namespace blink

namespace input {

class InputRouterClient;

// Performs stylus handwriting gesture handling for an InputRouterImpl.
class COMPONENT_EXPORT(INPUT) StylusHandwritingHandler {
 public:
  enum class GestureHandlingResult {
    kNotHandled,
    kConsumed,
    kForwardAsTapCancel,
  };

  explicit StylusHandwritingHandler(InputRouterClient* client);

  StylusHandwritingHandler(const StylusHandwritingHandler&) = delete;
  StylusHandwritingHandler& operator=(const StylusHandwritingHandler&) = delete;

  virtual ~StylusHandwritingHandler();

  virtual void OnTouchEvent(const blink::WebTouchEvent& event);
  virtual void ApplyTouchAction(cc::TouchAction touch_action);
  virtual GestureHandlingResult HandleGesture(
      const blink::WebGestureEvent& event,
      std::optional<cc::TouchAction> allowed_touch_action);
  virtual void EndStylusWriting();

  bool StylusWritingStarted() const { return stylus_writing_started_; }
  bool StartStylusWriting();

 private:
  raw_ptr<InputRouterClient> client_;
  bool stylus_writing_started_ = false;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_H_
