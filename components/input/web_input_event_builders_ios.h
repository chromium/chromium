// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_IOS_H_
#define COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_IOS_H_

#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

@class UIEvent;
@class UITouch;
@class UIView;

namespace input {

class COMPONENT_EXPORT(INPUT) WebKeyboardEventBuilder {
 public:
  static blink::WebKeyboardEvent Build(UIEvent* event);
};

class COMPONENT_EXPORT(INPUT) WebGestureEventBuilder {
 public:
  static blink::WebGestureEvent Build(UIEvent*, UIView*);
};

class COMPONENT_EXPORT(INPUT) WebTouchEventBuilder {
 public:
  static blink::WebTouchEvent Build(
      blink::WebInputEvent::Type type,
      UITouch* touch,
      UIEvent* event,
      UIView* view,
      const std::optional<gfx::Vector2dF>& view_offset);
};

}  // namespace input

#endif  // COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_IOS_H_
