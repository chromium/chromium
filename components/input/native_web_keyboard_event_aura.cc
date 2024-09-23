// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/native_web_keyboard_event.h"

#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

// We need to copy |os_event| in NativeWebKeyboardEvent because it is
// queued in RenderWidgetHost and may be passed and used
// RenderViewHostDelegate::HandledKeyboardEvent after the original aura
// event is destroyed.
ui::Event* CopyEvent(const ui::Event* event) {
  return event ? event->Clone().release() : nullptr;
}

int WebEventModifiersToEventFlags(int modifiers) {
  int flags = 0;
  if (modifiers & blink::WebInputEvent::kShiftKey) {
    flags |= ui::EF_SHIFT_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kControlKey) {
    flags |= ui::EF_CONTROL_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kAltKey) {
    flags |= ui::EF_ALT_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kMetaKey) {
    flags |= ui::EF_COMMAND_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kAltGrKey) {
    flags |= ui::EF_ALTGR_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kNumLockOn) {
    flags |= ui::EF_NUM_LOCK_ON;
  }
  if (modifiers & blink::WebInputEvent::kCapsLockOn) {
    flags |= ui::EF_CAPS_LOCK_ON;
  }
  if (modifiers & blink::WebInputEvent::kScrollLockOn) {
    flags |= ui::EF_SCROLL_LOCK_ON;
  }
  if (modifiers & blink::WebInputEvent::kLeftButtonDown) {
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kMiddleButtonDown) {
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kRightButtonDown) {
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kBackButtonDown) {
    flags |= ui::EF_BACK_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kForwardButtonDown) {
    flags |= ui::EF_FORWARD_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kIsAutoRepeat) {
    flags |= ui::EF_IS_REPEAT;
  }
  if (modifiers & blink::WebInputEvent::kIsTouchAccessibility) {
    flags |= ui::EF_TOUCH_ACCESSIBILITY;
  }
  return flags;
}

class TranslatedKeyEvent : public ui::KeyEvent {
 public:
  static TranslatedKeyEvent* Create(const blink::WebKeyboardEvent& web_event) {
    ui::EventType type = ui::EventType::kKeyReleased;
    bool is_char = false;
    if (web_event.GetType() == blink::WebInputEvent::Type::kChar) {
      is_char = true;
      type = ui::EventType::kKeyPressed;
    } else if (web_event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
               web_event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
      type = ui::EventType::kKeyPressed;
    }
    // look up the DomCode in the table because we can't trust the
    // WebKeyboardEvent as it came from the renderer.
    return new TranslatedKeyEvent(
        type, static_cast<ui::KeyboardCode>(web_event.windows_key_code),
        ui::KeycodeConverter::NativeKeycodeToDomCode(web_event.native_key_code),
        WebEventModifiersToEventFlags(web_event.GetModifiers()),
        web_event.dom_key, web_event.TimeStamp(), is_char);
  }

  // Event:
  std::unique_ptr<ui::Event> Clone() const override {
    return std::make_unique<TranslatedKeyEvent>(*this);
  }

 private:
  TranslatedKeyEvent(ui::EventType type,
                     ui::KeyboardCode keyboard_code,
                     ui::DomCode dom_code,
                     int flags,
                     ui::DomKey dom_key,
                     base::TimeTicks time,
                     bool is_char)
      : KeyEvent(type, keyboard_code, dom_code, flags, dom_key, time) {
    set_is_char(is_char);
  }
};

}  // namespace

using blink::WebKeyboardEvent;

namespace input {

NativeWebKeyboardEvent::NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                                               int modifiers,
                                               base::TimeTicks timestamp)
    : WebKeyboardEvent(type, modifiers, timestamp),
      os_event(nullptr),
      skip_if_unhandled(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const blink::WebKeyboardEvent& web_event,
    gfx::NativeView native_view)
    : WebKeyboardEvent(web_event), os_event(nullptr), skip_if_unhandled(false) {
  os_event = TranslatedKeyEvent::Create(web_event);
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : NativeWebKeyboardEvent(static_cast<ui::KeyEvent&>(*native_event)) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const ui::KeyEvent& key_event)
    : WebKeyboardEvent(ui::MakeWebKeyboardEvent(key_event)),
      os_event(CopyEvent(&key_event)),
      skip_if_unhandled(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(CopyEvent(other.os_event)),
      skip_if_unhandled(other.skip_if_unhandled) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const ui::KeyEvent& key_event,
                                               char16_t character)
    : WebKeyboardEvent(ui::MakeWebKeyboardEvent(key_event)),
      os_event(nullptr),
      skip_if_unhandled(false) {
  type_ = blink::WebInputEvent::Type::kChar;
  windows_key_code = character;
  text[0] = character;
  unmodified_text[0] = character;
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);
  delete os_event;
  os_event = CopyEvent(other.os_event);
  skip_if_unhandled = other.skip_if_unhandled;
  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  delete os_event;
}

}  // namespace input
