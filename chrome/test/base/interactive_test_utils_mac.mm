// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_constants.h"

namespace {

// A helper singleton for sending key events as Quartz events to the window
// server and waiting for them to arrive back to our NSApp.
class SendGlobalKeyEventsHelper {
 public:
  SendGlobalKeyEventsHelper();
  ~SendGlobalKeyEventsHelper();

  // Callback for MockCrApplication.
  void ObserveSendEvent(NSEvent* event);

  void OriginalSendEvent(id receiver, SEL selector, NSEvent* event) {
    scoped_swizzler_->InvokeOriginal<void, NSEvent*>(receiver, selector, event);
  }

  void SendGlobalKeyEventsAndWait(int key_code, int modifier_flags);

 private:
  void SendGlobalKeyEvent(int key_code,
                          CGEventFlags current_flags,
                          bool key_down);

  std::unique_ptr<base::mac::ScopedObjCClassSwizzler> scoped_swizzler_;
  base::ScopedCFTypeRef<CGEventSourceRef> event_source_;
  CGEventTapLocation event_tap_location_;
  base::RunLoop run_loop_;
  // First key code pressed in the event sequence. This is also the last key
  // code to be released and so it will be waited for.
  base::Optional<int> first_key_down_code_;

  DISALLOW_COPY_AND_ASSIGN(SendGlobalKeyEventsHelper);
};

SendGlobalKeyEventsHelper* g_global_key_events_helper = nullptr;

}  // namespace

@interface MockCrApplication : NSObject
@end

@implementation MockCrApplication

- (void)sendEvent:(NSEvent*)event {
  DCHECK(g_global_key_events_helper);
  g_global_key_events_helper->ObserveSendEvent(event);
  g_global_key_events_helper->OriginalSendEvent(self, _cmd, event);
}

@end

namespace {

SendGlobalKeyEventsHelper::SendGlobalKeyEventsHelper()
    : event_source_(CGEventSourceCreate(kCGEventSourceStateHIDSystemState)),
      event_tap_location_(kCGHIDEventTap) {
  DCHECK_EQ(nullptr, g_global_key_events_helper);
  g_global_key_events_helper = this;

  scoped_swizzler_ = std::make_unique<base::mac::ScopedObjCClassSwizzler>(
      [BrowserCrApplication class], [MockCrApplication class],
      @selector(sendEvent:));
}

SendGlobalKeyEventsHelper::~SendGlobalKeyEventsHelper() {
  DCHECK_EQ(this, g_global_key_events_helper);
  g_global_key_events_helper = nullptr;
}

void SendGlobalKeyEventsHelper::ObserveSendEvent(NSEvent* event) {
  DCHECK(first_key_down_code_);
  if (ui::IsKeyUpEvent(event) && [event keyCode] == *first_key_down_code_)
    run_loop_.Quit();
}

void SendGlobalKeyEventsHelper::SendGlobalKeyEventsAndWait(int key_code,
                                                           int modifier_flags) {
  CGEventFlags current_flags = 0;
  if ((modifier_flags & ui::EF_CONTROL_DOWN) != 0) {
    current_flags |= kCGEventFlagMaskControl;
    SendGlobalKeyEvent(kVK_Control, current_flags, true);
  }
  if ((modifier_flags & ui::EF_SHIFT_DOWN) != 0) {
    current_flags |= kCGEventFlagMaskShift;
    SendGlobalKeyEvent(kVK_Shift, current_flags, true);
  }
  if ((modifier_flags & ui::EF_ALT_DOWN) != 0) {
    current_flags |= kCGEventFlagMaskAlternate;
    SendGlobalKeyEvent(kVK_Option, current_flags, true);
  }
  if ((modifier_flags & ui::EF_COMMAND_DOWN) != 0) {
    current_flags |= kCGEventFlagMaskCommand;
    SendGlobalKeyEvent(kVK_Command, current_flags, true);
  }
  SendGlobalKeyEvent(key_code, current_flags, true);
  SendGlobalKeyEvent(key_code, current_flags, false);
  if ((modifier_flags & ui::EF_COMMAND_DOWN) != 0) {
    current_flags &= ~kCGEventFlagMaskCommand;
    SendGlobalKeyEvent(kVK_Command, current_flags, false);
  }
  if ((modifier_flags & ui::EF_ALT_DOWN) != 0) {
    current_flags &= ~kCGEventFlagMaskAlternate;
    SendGlobalKeyEvent(kVK_Option, current_flags, false);
  }
  if ((modifier_flags & ui::EF_SHIFT_DOWN) != 0) {
    current_flags &= ~kCGEventFlagMaskShift;
    SendGlobalKeyEvent(kVK_Shift, current_flags, false);
  }
  if ((modifier_flags & ui::EF_CONTROL_DOWN) != 0) {
    current_flags &= ~kCGEventFlagMaskControl;
    SendGlobalKeyEvent(kVK_Control, current_flags, false);
  }

  run_loop_.Run();
}

void SendGlobalKeyEventsHelper::SendGlobalKeyEvent(int key_code,
                                                   CGEventFlags current_flags,
                                                   bool key_down) {
  base::ScopedCFTypeRef<CGEventRef> key_event(
      CGEventCreateKeyboardEvent(event_source_, key_code, key_down));
  CGEventSetFlags(key_event, current_flags);

  // Starting in 10.14, CGEventPost() pops up a modal that asks the user to
  // confirm whether the app should be allowed to use accessibility APIs, which
  // hangs tests on the bots. https://crbug.com/904403
  DCHECK(base::mac::IsAtMostOS10_13());

  CGEventPost(event_tap_location_, key_event);
  if (key_down && !first_key_down_code_)
    first_key_down_code_ = key_code;
}

}  // namespace

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
  [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];

  base::scoped_nsobject<WindowedNSNotificationObserver> async_waiter;
  if (![window isKeyWindow]) {
    // Only wait when expecting a change to actually occur.
    async_waiter.reset([[WindowedNSNotificationObserver alloc]
        initForNotification:NSWindowDidBecomeKeyNotification
                     object:window]);
  }
  [window makeKeyAndOrderFront:nil];

  // Wait until |window| becomes key window, then make sure the shortcuts for
  // "Close Window" and "Close Tab" are updated.
  // This is because normal AppKit menu updating does not get invoked when
  // events are sent via ui_test_utils::SendKeyPressSync.
  BOOL notification_observed = [async_waiter wait];
  base::RunLoop().RunUntilIdle();  // There may be other events queued. Flush.
  NSMenu* file_menu = [[[NSApp mainMenu] itemWithTag:IDC_FILE_MENU] submenu];
  [[file_menu delegate] menuNeedsUpdate:file_menu];

  return !async_waiter || notification_observed;
}

void SendGlobalKeyEventsAndWait(int key_code, int modifier_flags) {
  SendGlobalKeyEventsHelper().SendGlobalKeyEventsAndWait(key_code,
                                                         modifier_flags);
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
                  base::ScopedCFTypeRef<CGEventRef>(CGEventCreateKeyboardEvent(
                      nullptr, known_modifier.key_code, false)));
      LOG(ERROR) << "Modifier " << known_modifier.name
                 << " is hanging down, and may cause problems for any "
                    "subsequent test.";
    }
  }
  return had_modifier;
}

}  // namespace ui_test_utils
