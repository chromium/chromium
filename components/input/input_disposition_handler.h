// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_DISPOSITION_HANDLER_H_
#define COMPONENTS_INPUT_INPUT_DISPOSITION_HANDLER_H_

#include "components/input/event_with_latency_info.h"
#include "components/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace input {

// Provided customized disposition response for input events.
class InputDispositionHandler {
 public:
  virtual ~InputDispositionHandler() {}

  // Called upon event ack receipt from the renderer.
  virtual void OnWheelEventAck(
      const MouseWheelEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
  virtual void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_DISPOSITION_HANDLER_H_
