// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/input/native_web_keyboard_event.h"

#import <AppKit/AppKit.h>

#include "base/containers/span.h"
#include "components/input/web_input_event_builders_mac.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"

namespace input {

namespace {

int modifiersForEvent(int modifiers) {
  int flags = 0;
  if (modifiers & blink::WebInputEvent::kControlKey) {
    flags |= NSEventModifierFlagControl;
  }
  if (modifiers & blink::WebInputEvent::kShiftKey) {
    flags |= NSEventModifierFlagShift;
  }
  if (modifiers & blink::WebInputEvent::kAltKey) {
    flags |= NSEventModifierFlagOption;
  }
  if (modifiers & blink::WebInputEvent::kMetaKey) {
    flags |= NSEventModifierFlagCommand;
  }
  if (modifiers & blink::WebInputEvent::kCapsLockOn) {
    flags |= NSEventModifierFlagCapsLock;
  }
  return flags;
}

size_t WebKeyboardEventTextLength(
    base::span<const char16_t, blink::WebKeyboardEvent::kTextLengthCap> text) {
  size_t text_length = 0;
  while (text_length < text.size() && text[text_length]) {
    ++text_length;
  }
  return text_length;
}

}  // namespace

NativeWebKeyboardEvent::NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                                               int modifiers,
                                               base::TimeTicks timestamp)
    : WebKeyboardEvent(type, modifiers, timestamp), skip_if_unhandled(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const blink::WebKeyboardEvent& web_event,
    gfx::NativeView native_view)
    : WebKeyboardEvent(web_event), skip_if_unhandled(false) {
  NSEventType type = NSEventTypeKeyUp;
  int flags = modifiersForEvent(web_event.GetModifiers());
  if (web_event.GetType() == blink::WebInputEvent::Type::kChar ||
      web_event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      web_event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
    type = NSEventTypeKeyDown;
  }
  size_t text_length = WebKeyboardEventTextLength(web_event.text);
  size_t unmod_text_length =
      WebKeyboardEventTextLength(web_event.unmodified_text);

  // Perform the reverse operation on type that was done in
  // UnmodifiedTextFromEvent(). Avoid using text_length as the control key may
  // cause Mac to set [NSEvent characters] to "\0" which for us is
  // indistinguishable from "".
  if (unmod_text_length == 0) {
    type = NSEventTypeFlagsChanged;
  }

  NSString* text = [[NSString alloc]
      initWithCharacters:reinterpret_cast<const UniChar*>(web_event.text.data())
                  length:text_length];
  NSString* unmodified_text =
      [[NSString alloc] initWithCharacters:reinterpret_cast<const UniChar*>(
                                               web_event.unmodified_text.data())
                                    length:unmod_text_length];

  os_event = base::apple::OwnedNSEvent([NSEvent
                 keyEventWithType:type
                         location:NSZeroPoint
                    modifierFlags:flags
                        timestamp:ui::EventTimeStampToSeconds(
                                      web_event.TimeStamp())
                     windowNumber:[[native_view.GetNativeNSView() window]
                                      windowNumber]
                          context:nil
                       characters:text
      charactersIgnoringModifiers:unmodified_text
                        isARepeat:NO
                          keyCode:web_event.native_key_code]);
  // The eventRef is necessary for MacOS code (like NSMenu) to work later in the
  // pipeline. As per documentation:
  // https://developer.apple.com/documentation/appkit/nsevent/1525143-eventref
  // "Other NSEvent objects create an EventRef when this property is first
  // accessed, if possible".
  [os_event.Get() eventRef];
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(WebKeyboardEventBuilder::Build(native_event.Get())),
      os_event(native_event),
      skip_if_unhandled(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const ui::KeyEvent& key_event)
    : WebKeyboardEvent(ui::MakeWebKeyboardEvent(key_event)),
      os_event(base::apple::OwnedNSEvent(key_event.native_event())),
      skip_if_unhandled(false) {}

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
