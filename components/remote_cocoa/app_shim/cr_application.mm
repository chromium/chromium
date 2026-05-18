// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/cr_application.h"

#include "base/apple/call_with_eh_frame.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key.h"

@implementation CrApplication {
  BOOL _handlingSendEvent;
}

- (BOOL)isHandlingSendEvent {
  return _handlingSendEvent;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  _handlingSendEvent = handlingSendEvent;
}

- (BOOL)sendAction:(SEL)anAction to:(id)aTarget from:(id)sender {
  // The Dock menu contains an automagic section where you can select
  // amongst open windows.  If a window is closed via JavaScript while
  // the menu is up, the menu item for that window continues to exist.
  // When a window is selected this method is called with the
  // now-freed window as |aTarget|.  Short-circuit the call if
  // |aTarget| is not a valid window.
  if (anAction == @selector(_selectWindow:)) {
    // Not using -[NSArray containsObject:] because |aTarget| may be a
    // freed object.
    BOOL found = NO;
    for (NSWindow* window in [self windows]) {
      if (window == aTarget) {
        found = YES;
        break;
      }
    }
    if (!found) {
      return NO;
    }
  }

  // When a Cocoa control is wired to a freed object, we get crashers
  // in the call to |super| with no useful information in the
  // backtrace.  Attempt to add some useful information.

  // If the action is something generic like -commandDispatch:, then
  // the tag is essential.
  NSInteger tag = 0;
  if ([sender isKindOfClass:[NSControl class]]) {
    tag = [sender tag];
    if (tag == 0 || tag == -1) {
      tag = [sender selectedTag];
    }
  } else if ([sender isKindOfClass:[NSMenuItem class]]) {
    tag = [sender tag];
  }

  NSString* actionString = NSStringFromSelector(anAction);
  std::string value = base::StringPrintf(
      "%s tag %ld sending %s to %p",
      base::SysNSStringToUTF8([sender className]).c_str(),
      static_cast<long>(tag), base::SysNSStringToUTF8(actionString).c_str(),
      aTarget);

  static crash_reporter::CrashKeyString<256> sendActionKey("sendaction");
  crash_reporter::ScopedCrashKeyString scopedKey(&sendActionKey, value);

  __block BOOL rv;
  base::apple::CallWithEHFrame(^{
    rv = [super sendAction:anAction to:aTarget from:sender];
  });
  return rv;
}

- (NSEvent*)nextEventMatchingMask:(NSEventMask)mask
                        untilDate:(NSDate*)expiration
                           inMode:(NSString*)mode
                          dequeue:(BOOL)dequeue {
  __block NSEvent* event = nil;
  base::apple::CallWithEHFrame(^{
    event = [super nextEventMatchingMask:mask
                               untilDate:expiration
                                  inMode:mode
                                 dequeue:dequeue];
  });
  return event;
}

@end
