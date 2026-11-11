// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/cr_application.h"

#include <Carbon/Carbon.h>  // for <HIToolbox/Events.h>

#include "base/apple/call_with_eh_frame.h"
#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "components/remote_cocoa/app_shim/features.h"

namespace {

// Calling -[NSEvent description] is rather slow to build up the event
// description. The description is stored in a crash key to aid debugging, so
// this helper function constructs a shorter, but still useful, description.
// See <https://crbug.com/40542574>.
std::string DescriptionForNSEvent(NSEvent* event) {
  std::string desc = base::StringPrintf(
      "NSEvent type=%ld modifierFlags=0x%lx locationInWindow=(%g,%g)",
      event.type, event.modifierFlags, event.locationInWindow.x,
      event.locationInWindow.y);
  switch (event.type) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp: {
      // Some NSEvents return a string with NUL in event.characters, see
      // <https://crbug.com/41379586>. To make matters worse, in rare cases,
      // NSEvent.characters or NSEvent.charactersIgnoringModifiers can throw an
      // NSException complaining that "TSMProcessRawKeyCode failed". Since we're
      // trying to gather a crash key here, if that exception happens, just
      // remark that it happened and continue rather than crashing the browser.
      std::string characters, unmodified_characters;
      @try {
        characters = base::SysNSStringToUTF8([event.characters
            stringByReplacingOccurrencesOfString:@"\0"
                                      withString:@"\\x00"]);
        unmodified_characters =
            base::SysNSStringToUTF8([event.charactersIgnoringModifiers
                stringByReplacingOccurrencesOfString:@"\0"
                                          withString:@"\\x00"]);
      } @catch (id exception) {
        characters = "(exception)";
        unmodified_characters = "(exception)";
      }
      desc += base::StringPrintf(
          " keyCode=0x%d ARepeat=%d characters='%s' unmodifiedCharacters='%s'",
          event.keyCode, event.ARepeat, characters.c_str(),
          unmodified_characters.c_str());
      break;
    }
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeRightMouseUp:
      desc += base::StringPrintf(" buttonNumber=%ld clickCount=%ld",
                                 event.buttonNumber, event.clickCount);
      break;
    case NSEventTypeAppKitDefined:
    case NSEventTypeSystemDefined:
    case NSEventTypeApplicationDefined:
    case NSEventTypePeriodic:
      desc += base::StringPrintf(" subtype=%d data1=%ld data2=%ld",
                                 event.subtype, event.data1, event.data2);
      break;
    default:
      break;
  }
  return desc;
}

}  // namespace

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

  // Clicking the Full Screen heading in the green-button menu can crash on
  // macOS 27.0. (https://crbug.com/565084600, FB24915584). Ignore
  // -submenuAction: when the target does not respond.
  if (base::mac::MacOSVersion() >= 27'00'00 &&
      base::mac::MacOSVersion() < 27'02'00 &&
      anAction == @selector(submenuAction:) &&
      ![aTarget respondsToSelector:anAction]) {
    return NO;
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

- (void)sendEvent:(NSEvent*)event {
  static crash_reporter::CrashKeyString<256> nseventKey("nsevent");
  crash_reporter::ScopedCrashKeyString scopedKey(&nseventKey,
                                                 DescriptionForNSEvent(event));

  base::apple::CallWithEHFrame(^{
    base::mac::ScopedSendingEvent sendingEventScoper;

    BOOL sendEventToKeyWindow = NO;
    if (event.type == NSEventTypeKeyDown &&
        (event.keyCode == kVK_JIS_Eisu || event.keyCode == kVK_JIS_Kana)) {
      // Mac Eisu and Kana keydown events are by default swallowed by sendEvent
      // and sent directly to IME, which prevents ui keydown events from firing.
      // These events need to be sent to [NSApp keyWindow] for handling.
      sendEventToKeyWindow = YES;
    } else if (event.type == NSEventTypeKeyUp &&
               (event.modifierFlags & NSEventModifierFlagCommand) &&
               // `keyWindow` must be nil-checked before querying `FeatureList`:
               // during app shim bootstrap (before `FinalizeFeatureState()`
               // runs and before any windows exist), querying non-early-allowed
               // features triggers a CHECK crash.
               self.keyWindow &&
               base::FeatureList::IsEnabled(
                   remote_cocoa::features::kForwardCmdKeyUpEventsToWindow)) {
      // The base NSApplication implementation of sendEvent: swallows keyUp
      // events if the command modifier is present. We work around this by
      // forwarding them to the key window to ensure all keyUp events are
      // reported and handled (crbug.com/407598429, crbug.com/438807261).
      sendEventToKeyWindow = YES;
    }

    if (sendEventToKeyWindow) {
      [self.keyWindow sendEvent:event];
    } else {
      [super sendEvent:event];
    }
  });
}

@end
