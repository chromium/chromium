// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/input/native_web_keyboard_event.h"

#include "components/input/web_input_event_builders_ios.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace input {

NativeWebKeyboardEvent::NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                                               int modifiers,
                                               base::TimeTicks timestamp)
    : WebKeyboardEvent(type, modifiers, timestamp), skip_if_unhandled(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const blink::WebKeyboardEvent& web_event,
    gfx::NativeView native_view)
    : WebKeyboardEvent(web_event), skip_if_unhandled(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(WebKeyboardEventBuilder::Build(native_event.Get())),
      os_event(native_event),
      skip_if_unhandled(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const ui::KeyEvent& key_event)
    : NativeWebKeyboardEvent(
          base::apple::OwnedUIEvent(key_event.native_event())) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(other.os_event),
      skip_if_unhandled(other.skip_if_unhandled) {}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  os_event = other.os_event;

  skip_if_unhandled = other.skip_if_unhandled;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() = default;

}  // namespace input
