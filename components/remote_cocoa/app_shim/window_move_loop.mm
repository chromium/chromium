// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/window_move_loop.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
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

namespace {

// This class addresses a macOS 14 issue where child windows don't follow
// the parent during tab dragging.
class ChildWindowMover {
 public:
  ChildWindowMover(NSWindow* window) : window_(window) {
    initial_parent_origin_ = gfx::Point(window.frame.origin);
    for (NSWindow* child in window.childWindows) {
      initial_origins_.emplace_back(child, child.frame.origin);
    }
  }

  // Moves child windows based on a parent origin offset relative to their
  // initial origins captured at the construction of this class.
  void MoveByOriginOffset() {
    if (!window_) {
      return;
    }

    gfx::Point parent_origin = gfx::Point(window_.frame.origin);
    gfx::Vector2d origin_offset(parent_origin.x() - initial_parent_origin_.x(),
                                parent_origin.y() - initial_parent_origin_.y());

    for (const auto& [child, initial_origin] : initial_origins_) {
      if (!child || child.parentWindow != window_) {
        continue;
      }

      gfx::Point expected_origin = initial_origin + origin_offset;
      // On macOS 14, child windows occasionally fail to follow their parent
      // during tab dragging. A workaround for this issue is to temporarily
      // remove the child window, set its frame origin, and then re-add it.
      [window_ removeChildWindow:child];
      [child
          setFrameOrigin:NSMakePoint(expected_origin.x(), expected_origin.y())];
      [window_ addChildWindow:child ordered:NSWindowAbove];
    }
  }

 private:
  NSWindow* __weak window_;
  std::vector<std::pair<NSWindow * __weak, gfx::Point>> initial_origins_;
  gfx::Point initial_parent_origin_;
};

}  // namespace

namespace remote_cocoa {

CocoaWindowMoveLoop::CocoaWindowMoveLoop(NativeWidgetNSWindowBridge* owner,
                                         const NSPoint& initial_mouse_in_screen)
    : owner_(owner),
      initial_mouse_in_screen_(initial_mouse_in_screen),
      weak_factory_(this) {}

CocoaWindowMoveLoop::~CocoaWindowMoveLoop() {
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
  __block ChildWindowMover child_window_mover(window);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  // Will be retained by the monitor handler block.
  WeakCocoaWindowMoveLoop* weak_cocoa_window_move_loop =
      [[WeakCocoaWindowMoveLoop alloc]
          initWithWeakPtr:weak_factory_.GetWeakPtr()];

  __block BOOL has_moved = NO;
  screen_disabler_ = std::make_unique<gfx::ScopedCocoaDisableScreenUpdates>();

  // Esc keypress is handled by EscapeTracker, which is installed by
  // TabDragController.
  NSEventMask mask = NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged |
                     NSEventMaskMouseMoved;
  auto handler = ^NSEvent*(NSEvent* event) {
    // The docs say this always runs on the main thread, but if it didn't,
    // it would explain https://crbug.com/876493, so let's make sure.
    CHECK(NSThread.isMainThread);

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
      gfx::Vector2d mouse_offset(
          mouse_in_screen.x - initial_mouse_in_screen_.x,
          mouse_in_screen.y - initial_mouse_in_screen_.y);
      NSRect ns_frame =
          NSOffsetRect(initial_frame, mouse_offset.x(), mouse_offset.y());
      [window setFrame:ns_frame display:NO animate:NO];
      child_window_mover.MoveByOriginOffset();
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
