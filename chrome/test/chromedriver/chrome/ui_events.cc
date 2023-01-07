// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/ui_events.h"

#include "base/check.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

MouseEvent::MouseEvent(MouseEventType type,
                       MouseButton button,
                       int x,
                       int y,
                       int modifiers,
                       int buttons,
                       int click_count)
    : type(type),
      button(button),
      x(x),
      y(y),
      modifiers(modifiers),
      buttons(buttons),
      click_count(click_count),
      force(0.0),
      tangentialPressure(0.0),
      tiltX(0),
      tiltY(0),
      twist(0),
      pointer_type(kMouse) {}

MouseEvent::MouseEvent(const MouseEvent& other) = default;

MouseEvent::~MouseEvent() {}

TouchEvent::TouchEvent() : TouchEvent(kPause, 0, 0) {}

TouchEvent::TouchEvent(TouchEventType type, int x, int y)
    : type(type),
      x(x),
      y(y),
      radiusX(1.0),
      radiusY(1.0),
      rotationAngle(0.0),
      force(1.0),
      tangentialPressure(0.0),
      tiltX(0),
      tiltY(0),
      twist(0),
      id(0),
      dispatch(true) {}

TouchEvent::TouchEvent(const TouchEvent& other) = default;

TouchEvent::~TouchEvent() {}

KeyEvent::KeyEvent()
    : type(kInvalidEventType),
      modifiers(0),
      modified_text(std::string()),
      unmodified_text(std::string()),
      key(std::string()),
      key_code(ui::VKEY_UNKNOWN),
      location(0),
      code(),
      is_from_action(false) {}

KeyEvent::KeyEvent(const KeyEvent& that)
    : type(that.type),
      modifiers(that.modifiers),
      modified_text(that.modified_text),
      unmodified_text(that.unmodified_text),
      key(that.key),
      key_code(that.key_code),
      location(that.location),
      code(that.code),
      is_from_action(that.is_from_action) {}

KeyEvent::~KeyEvent() {}

KeyEventBuilder::KeyEventBuilder() {}

KeyEventBuilder::~KeyEventBuilder() {}

KeyEventBuilder* KeyEventBuilder::SetType(KeyEventType type) {
  key_event_.type = type;
  return this;
}

KeyEventBuilder* KeyEventBuilder::AddModifiers(int modifiers) {
  key_event_.modifiers |= modifiers;
  UpdateKeyString();
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetModifiers(int modifiers) {
  key_event_.modifiers = modifiers;
  UpdateKeyString();
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetText(const std::string& unmodified_text,
                                          const std::string& modified_text) {
  key_event_.unmodified_text = unmodified_text;
  key_event_.modified_text = modified_text;
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetKeyCode(ui::KeyboardCode key_code) {
  key_event_.key_code = key_code;
  UpdateKeyString();
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetLocation(int location) {
  key_event_.location = location;
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetDefaultKey(const std::string& key) {
  if (key_event_.key.size() == 0)
    key_event_.key = key;
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetCode(const std::string& code) {
  key_event_.code = code;
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetIsFromAction() {
  key_event_.is_from_action = true;
  return this;
}

KeyEvent KeyEventBuilder::Build() {
  DCHECK(key_event_.type != kInvalidEventType);
  return key_event_;
}

void KeyEventBuilder::Generate(std::vector<KeyEvent>* key_events) {
  key_events->push_back(SetType(kRawKeyDownEventType)->Build());
  if (key_event_.modified_text.length() || key_event_.unmodified_text.length())
    key_events->push_back(SetType(kCharEventType)->Build());
  key_events->push_back(SetType(kKeyUpEventType)->Build());
}

void KeyEventBuilder::UpdateKeyString() {
  ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(key_event_.key_code);
  int flags = ui::EF_NONE;
  if (key_event_.modifiers & kAltKeyModifierMask)
    flags |= ui::EF_ALT_DOWN;
  if (key_event_.modifiers & kControlKeyModifierMask)
    flags |= ui::EF_CONTROL_DOWN;
  if (key_event_.modifiers & kMetaKeyModifierMask)
    flags |= ui::EF_COMMAND_DOWN;
  if (key_event_.modifiers & kShiftKeyModifierMask)
    flags |= ui::EF_SHIFT_DOWN;
  if (key_event_.modifiers & kNumLockKeyModifierMask)
    flags |= ui::EF_NUM_LOCK_ON;
  ui::DomKey dom_key;
  ui::KeyboardCode ignored;
  if (ui::DomCodeToUsLayoutDomKey(dom_code, flags, &dom_key, &ignored))
    key_event_.key = ui::KeycodeConverter::DomKeyToKeyString(dom_key);
  else
    key_event_.key = std::string();
}
