// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_MAC_H_
#define COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_MAC_H_

#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

@class NSEvent;
@class NSView;

namespace input {

class COMPONENT_EXPORT(INPUT) WebKeyboardEventBuilder {
 public:
  static blink::WebKeyboardEvent Build(NSEvent* event);
};

class COMPONENT_EXPORT(INPUT) WebMouseEventBuilder {
 public:
  static blink::WebMouseEvent Build(
      NSEvent* event,
      NSView* view,
      blink::WebPointerProperties::PointerType pointerType =
          blink::WebPointerProperties::PointerType::kMouse,
      bool unacceleratedMovement = false);
};

class COMPONENT_EXPORT(INPUT) WebMouseWheelEventBuilder {
 public:
  static blink::WebMouseWheelEvent Build(NSEvent* event, NSView* view);
};

class COMPONENT_EXPORT(INPUT) WebGestureEventBuilder {
 public:
  static blink::WebGestureEvent Build(NSEvent*, NSView*);
};

class COMPONENT_EXPORT(INPUT) WebTouchEventBuilder {
 public:
  static blink::WebTouchEvent Build(NSEvent* event, NSView* view);
};

}  // namespace input

#endif  // COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_MAC_H_
