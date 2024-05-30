// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/input/native_web_keyboard_event.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

#include "components/input/web_input_event_builders_mac.h"
#import "testing/gtest_mac.h"
#include "ui/events/blink/web_input_event.h"

// Going from NSEvent to WebKeyboardEvent and back should round trip.
TEST(NativeWebKeyboardEventMac, CtrlCmdSpaceKeyDownRoundTrip) {
  NSEvent* ns_event =
      [NSEvent keyEventWithType:NSEventTypeKeyDown
                             location:NSZeroPoint
                        modifierFlags:NSEventModifierFlagControl |
                                      NSEventModifierFlagCommand
                            timestamp:0
                         windowNumber:0
                              context:nil
                           characters:@"\0"  // The control modifier results in
                                             // ' ' being bitmasked with 0x1F
                                             // which == 0x00.
          charactersIgnoringModifiers:@" "
                            isARepeat:NO
                              keyCode:kVK_Space];
  blink::WebKeyboardEvent web_event =
      input::WebKeyboardEventBuilder::Build(ns_event);
  input::NativeWebKeyboardEvent native_event(web_event, gfx::NativeView());

  NSEvent* round_trip_ns_event = native_event.os_event.Get();
  EXPECT_EQ(round_trip_ns_event.type, ns_event.type);
  EXPECT_EQ(round_trip_ns_event.modifierFlags, ns_event.modifierFlags);
  EXPECT_EQ(round_trip_ns_event.keyCode, ns_event.keyCode);
}
