// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/window_move_loop.h"
#include <memory>

#include "base/debug/stack_trace.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/display/screen.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// When event monitors process the events the full list of monitors is cached,
// and if we unregister the event monitor that's at the end of the list while
// processing the first monitor's handler -- the callback for the unregistered
// monitor will still be called even though it's unregistered. This will result
// in dereferencing an invalid pointer.
//
// WeakCocoaWindowMoveLoop is retained by the event monitor and stores weak
// pointer for the CocoaWindowMoveLoop, so there will be no invalid memory
// access.
@interface WeakCocoaWindowMoveLoop : NSObject {
 @private
  base::WeakPtr<remote_cocoa::CocoaWindowMoveLoop> _weak;
}
@end

@implementation WeakCocoaWindowMoveLoop
- (instancetype)initWithWeakPtr:
    (const base::WeakPtr<remote_cocoa::CocoaWindowMoveLoop>&)weak {
  if ((self = [super init])) {
    _weak = weak;
  }
  return self;
}

- (base::WeakPtr<remote_cocoa::CocoaWindowMoveLoop>&)weak {
  return _weak;
}
@end

namespace remote_cocoa {

CocoaWindowMoveLoop::CocoaWindowMoveLoop(NativeWidgetNSWindowBridge* owner,
                                         const NSPoint& initial_mouse_in_screen)
    : owner_(owner),
      initial_mouse_in_screen_(initial_mouse_in_screen),
      weak_factory_(this) {}

CocoaWindowMoveLoop::~CocoaWindowMoveLoop() {
  // Record the address and stack to help catch https://crbug.com/876493.
  static crash_reporter::CrashKeyString<19> address_key("move_loop_address");
  address_key.Set(base::StringPrintf("%p", this));

  static crash_reporter::CrashKeyString<1024> stack_key("move_loop_stack");
  crash_reporter::SetCrashKeyStringToStackTrace(&stack_key,
                                                base::debug::StackTrace());
  // Handle the pathological case, where |this| is destroyed while running.
  if (exit_reason_ref_) {
    *exit_reason_ref_ = WINDOW_DESTROYED;
    std::move(quit_closure_).Run();
  }

  owner_ = nullptr;
}

bool CocoaWindowMoveLoop::Run() {
  LoopExitReason exit_reason = ENDED_EXTERNALLY;
  exit_reason_ref_ = &exit_reason;
  NSWindow* window = owner_->ns_window();
  const NSRect initial_frame = [window frame];

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  // Will be retained by the monitor handler block.
  WeakCocoaWindowMoveLoop* weak_cocoa_window_move_loop =
      [[[WeakCocoaWindowMoveLoop alloc]
          initWithWeakPtr:weak_factory_.GetWeakPtr()] autorelease];

  __block BOOL has_moved = NO;
  screen_disabler_ = std::make_unique<gfx::ScopedCocoaDisableScreenUpdates>();

  // Esc keypress is handled by EscapeTracker, which is installed by
  // TabDragController.
  NSEventMask mask = NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged |
                     NSEventMaskMouseMoved;
  auto handler = ^NSEvent*(NSEvent* event) {
    // The docs say this always runs on the main thread, but if it didn't,
    // it would explain https://crbug.com/876493, so let's make sure.
    CHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());

    CocoaWindowMoveLoop* strong = [weak_cocoa_window_move_loop weak].get();
    if (!strong || !strong->exit_reason_ref_) {
      // By this point CocoaWindowMoveLoop was deleted while processing this
      // same event, and this event monitor was not unregistered in time. See
      // the WeakCocoaWindowMoveLoop comment above.
      // Continue processing the event.
      return event;
    }

    if ([event type] == NSEventTypeLeftMouseDragged) {
      const NSPoint mouse_in_screen = [NSEvent mouseLocation];

      NSRect ns_frame = NSOffsetRect(
          initial_frame, mouse_in_screen.x - initial_mouse_in_screen_.x,
          mouse_in_screen.y - initial_mouse_in_screen_.y);
      [window setFrame:ns_frame display:NO animate:NO];
      // `setFrame:...` may have destroyed `this`, so do the weak check again.
      bool is_valid = [weak_cocoa_window_move_loop weak].get() == strong;
      if (is_valid && !has_moved) {
        has_moved = YES;
        strong->screen_disabler_.reset();
      }

      return event;
    }

    // In theory, we shouldn't see any kind of NSEventTypeMouseMoved, but if we
    // see one and the left button isn't pressed, we know for a fact that we
    // missed a NSEventTypeLeftMouseUp.
    BOOL unexpectedMove = [event type] == NSEventTypeMouseMoved &&
                          ([NSEvent pressedMouseButtons] & 1) != 1;
    if (unexpectedMove || [event type] == NSEventTypeLeftMouseUp) {
      *strong->exit_reason_ref_ = MOUSE_UP;
      std::move(strong->quit_closure_).Run();
    }
    return event;  // Process the MouseUp.
  };
  id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask
                                                     handler:handler];

  run_loop.Run();
  [NSEvent removeMonitor:monitor];

  if (exit_reason != WINDOW_DESTROYED && exit_reason != ENDED_EXTERNALLY) {
    exit_reason_ref_ = nullptr;  // Ensure End() doesn't replace the reason.
    owner_->EndMoveLoop();       // Deletes |this|.
  }

  return exit_reason == MOUSE_UP;
}

void CocoaWindowMoveLoop::End() {
  screen_disabler_.reset();
  if (exit_reason_ref_) {
    DCHECK_EQ(*exit_reason_ref_, ENDED_EXTERNALLY);
    // Ensure the destructor doesn't replace the reason.
    exit_reason_ref_ = nullptr;
    std::move(quit_closure_).Run();
  }
}

}  // namespace remote_cocoa
