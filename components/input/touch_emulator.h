// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_TOUCH_EMULATOR_H_
#define COMPONENTS_INPUT_TOUCH_EMULATOR_H_

#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/events/gesture_detection/gesture_provider.h"

namespace input {

class RenderWidgetHostViewInput;

// Emulates touch input. See TouchEmulator::Mode for more details.
class COMPONENT_EXPORT(INPUT) TouchEmulator : public ui::GestureProviderClient {
 public:
  enum class Mode {
    // Emulator will consume incoming mouse events and transform them
    // into touches and gestures.
    kEmulatingTouchFromMouse,
    // Emulator will not consume incoming mouse events and instead will
    // wait for manually injected touch events.
    kInjectingTouchEvents
  };

  // Call when device scale factor changes.
  virtual void SetDeviceScaleFactor(float device_scale_factor) = 0;

  // See GestureProvider::SetDoubleTapSupportForPageEnabled.
  virtual void SetDoubleTapSupportForPageEnabled(bool enabled) = 0;

  // Note that TouchEmulator should always listen to touch events and their acks
  // (even in disabled state) to track native stream presence.
  virtual bool IsEnabled() const = 0;

  virtual bool HandleTouchEvent(const blink::WebTouchEvent& event) = 0;

  virtual void OnGestureEventAck(
      const blink::WebGestureEvent& event,
      RenderWidgetHostViewInput* target_view) = 0;

  // Called to notify the TouchEmulator when a view is destroyed.
  virtual void OnViewDestroyed(
      RenderWidgetHostViewInput* destroyed_view) = 0;

  // Returns |true| if the event ack was consumed. Consumed ack should not
  // propagate any further.
  virtual bool HandleTouchEventAck(
      const blink::WebTouchEvent& event,
      blink::mojom::InputEventResultState ack_result) = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_TOUCH_EMULATOR_H_
