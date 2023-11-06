// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_constants.h"

@interface NSApplication (Private)
// (Apparently) forces the application to activate itself.
- (void)_handleActivatedEvent:(id)arg1;
@end

namespace ui_test_utils {

void HideNativeWindow(gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();
  [window orderOut:nil];
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();
  // Make sure an unbundled program can get the input focus.
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  TransformProcessType(&psn,kProcessTransformToForegroundApplication);
  // We used to call [NSApp activateIgnoringOtherApps:YES] but this
  // would not reliably activate the app, causing the window to never
  // become key. This bit of private API appears to be the secret
  // incantation that gets us what we want. See https://crbug.com/1215570.
  [NSApplication.sharedApplication _handleActivatedEvent:nil];

  WindowedNSNotificationObserver* async_waiter;
  if (!window.keyWindow) {
    // Only wait when expecting a change to actually occur.
    async_waiter = [[WindowedNSNotificationObserver alloc]
        initForNotification:NSWindowDidBecomeKeyNotification
                     object:window];
  }
  [window makeKeyAndOrderFront:nil];

  // Wait until |window| becomes key window, then make sure the shortcuts for
  // "Close Window" and "Close Tab" are updated.
  // This is because normal AppKit menu updating does not get invoked when
  // events are sent via ui_test_utils::SendKeyPressSync.
  BOOL notification_observed = [async_waiter wait];
  base::RunLoop().RunUntilIdle();  // There may be other events queued. Flush.
  NSMenu* file_menu = [[NSApp.mainMenu itemWithTag:IDC_FILE_MENU] submenu];
  [file_menu.delegate menuNeedsUpdate:file_menu];

  return !async_waiter || notification_observed;
}

bool ClearKeyEventModifiers() {
  static constexpr struct {
    CGEventFlags flag_mask;
    int key_code;
    const char* name;
  } kKnownModifiers[] = {
      {kCGEventFlagMaskCommand, kVK_Command, "Cmd"},
      {kCGEventFlagMaskShift, kVK_Shift, "Shift"},
      {kCGEventFlagMaskAlternate, kVK_Option, "Option"},
      // Expand as needed.
  };
  CGEventFlags event_flags =
      CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
  bool had_modifier = false;
  for (const auto& known_modifier : kKnownModifiers) {
    if (known_modifier.flag_mask & event_flags) {
      had_modifier = true;
      CGEventPost(kCGSessionEventTap,
                  base::apple::ScopedCFTypeRef<CGEventRef>(
                      CGEventCreateKeyboardEvent(
                          nullptr, known_modifier.key_code, false))
                      .get());
      LOG(ERROR) << "Modifier " << known_modifier.name
                 << " is hanging down, and may cause problems for any "
                    "subsequent test.";
    }
  }
  return had_modifier;
}

}  // namespace ui_test_utils
