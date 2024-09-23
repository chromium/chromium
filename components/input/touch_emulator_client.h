// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_TOUCH_EMULATOR_CLIENT_H_
#define COMPONENTS_INPUT_TOUCH_EMULATOR_CLIENT_H_

#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/base/ui_base_types.h"

namespace ui {
class Cursor;
}

namespace input {

class RenderWidgetHostViewInput;

// Emulates touch input with mouse and keyboard.
class TouchEmulatorClient {
 public:
  virtual ~TouchEmulatorClient() = default;

  virtual void ForwardEmulatedGestureEvent(
      const blink::WebGestureEvent& event) = 0;
  virtual void ForwardEmulatedTouchEvent(
      const blink::WebTouchEvent& event,
      RenderWidgetHostViewInput* target) = 0;
  virtual void SetCursor(const ui::Cursor& cursor) = 0;
  // |target| is the view associated with the corresponding input event.
  virtual void ShowContextMenuAtPoint(
      const gfx::Point& point,
      const ui::MenuSourceType source_type,
      RenderWidgetHostViewInput* target) = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_TOUCH_EMULATOR_CLIENT_H_
