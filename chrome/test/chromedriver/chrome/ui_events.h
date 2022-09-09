// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_UI_EVENTS_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_UI_EVENTS_H_

#include <string>
#include <vector>

#include "ui/events/keycodes/keyboard_codes.h"

// Specifies the type of the mouse event.
enum MouseEventType {
  kPressedMouseEventType = 0,
  kReleasedMouseEventType,
  kMovedMouseEventType,
  kWheelMouseEventType,
  kPauseMouseEventType
};

// Specifies the mouse buttons.
enum MouseButton {
  kLeftMouseButton = 0,
  kMiddleMouseButton,
  kRightMouseButton,
  kBackMouseButton,
  kForwardMouseButton,
  kNoneMouseButton
};

// Specifies the event's pointer type.
enum PointerType { kMouse = 0, kPen };

// Specifies the origin of pointer location.
enum OriginType { kViewPort, kPointer, kElement };

struct MouseEvent {
  MouseEvent(MouseEventType type,
             MouseButton button,
             int x,
             int y,
             int modifiers,
             int buttons,
             int click_count);
  MouseEvent(const MouseEvent& other);
  ~MouseEvent();

  MouseEventType type;
  MouseButton button;
  int x;
  int y;
  int modifiers;
  int buttons;
  // |click_count| should not be negative.
  int click_count;
  int delta_x;
  int delta_y;
  double force;
  double tangentialPressure;
  int tiltX;
  int tiltY;
  int twist;
  PointerType pointer_type;
};

// Specifies the type of the touch event.
enum TouchEventType {
  kTouchStart = 0,
  kTouchEnd,
  kTouchMove,
  kTouchCancel,
  kPause
};

struct TouchEvent {
  TouchEvent();
  TouchEvent(TouchEventType type,
             int x,
             int y);
  TouchEvent(const TouchEvent& other);
  ~TouchEvent();

  TouchEventType type;
  int x;
  int y;
  double radiusX;
  double radiusY;
  double rotationAngle;
  double force;
  double tangentialPressure;
  int tiltX;
  int tiltY;
  int twist;
  int id;
  bool dispatch;
};

// Specifies the type of the keyboard event.
enum KeyEventType {
  kKeyDownEventType = 0,
  kKeyUpEventType,
  kRawKeyDownEventType,
  kCharEventType,
  kPauseEventType,
  kInvalidEventType  // used by KeyEventBuilder
};

// Specifies modifier keys as stated in
// third_party/WebKit/Source/WebCore/inspector/Inspector.json.
// Notice: |kNumLockKeyModifierMask| is for usage in the key_converter.cc
//         and keycode_text_conversion_x.cc only, not for inspector.
enum KeyModifierMask {
  kAltKeyModifierMask = 1 << 0,
  kControlKeyModifierMask = 1 << 1,
  kMetaKeyModifierMask = 1 << 2,
  kShiftKeyModifierMask = 1 << 3,
  kNumLockKeyModifierMask = 1 << 4
};

struct KeyEvent {
  KeyEvent();
  KeyEvent(const KeyEvent& that);
  ~KeyEvent();

  KeyEventType type;
  int modifiers;
  std::string modified_text;
  std::string unmodified_text;
  std::string key;
  ui::KeyboardCode key_code;
  int location;
  std::string code;
  bool is_from_action;
};

class KeyEventBuilder {
 public:
  KeyEventBuilder();

  KeyEventBuilder(const KeyEventBuilder&) = delete;
  KeyEventBuilder& operator=(const KeyEventBuilder&) = delete;

  virtual ~KeyEventBuilder();

  KeyEventBuilder* SetType(KeyEventType type);
  KeyEventBuilder* AddModifiers(int modifiers);
  KeyEventBuilder* SetModifiers(int modifiers);
  KeyEventBuilder* SetText(const std::string& unmodified_text,
                           const std::string& modified_text);
  KeyEventBuilder* SetKeyCode(ui::KeyboardCode key_code);
  KeyEventBuilder* SetLocation(int location);
  KeyEventBuilder* SetDefaultKey(const std::string& key);
  KeyEventBuilder* SetCode(const std::string& key);
  KeyEventBuilder* SetIsFromAction();
  KeyEvent Build();
  void Generate(std::vector<KeyEvent>* key_events);

 private:
  void UpdateKeyString();

  KeyEvent key_event_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_UI_EVENTS_H_
