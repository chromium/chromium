// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_

#import <Cocoa/Cocoa.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/mac/scoped_cocoa_disable_screen_updates.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;

// Used by NativeWidgetNSWindowBridge when dragging detached tabs.
class CocoaWindowMoveLoop {
 public:
  CocoaWindowMoveLoop(NativeWidgetNSWindowBridge* owner,
                      const NSPoint& initial_mouse_in_screen);

  CocoaWindowMoveLoop(const CocoaWindowMoveLoop&) = delete;
  CocoaWindowMoveLoop& operator=(const CocoaWindowMoveLoop&) = delete;

  ~CocoaWindowMoveLoop();

  // Initiates the drag until a mouse up event is observed, or End() is called.
  // Returns true if a mouse up event ended the loop.
  bool Run();
  void End();

 private:
  enum LoopExitReason {
    ENDED_EXTERNALLY,
    MOUSE_UP,
    WINDOW_DESTROYED,
  };

  raw_ptr<NativeWidgetNSWindowBridge> owner_;  // Weak. Owns this.

  // Initial mouse location at the time before the CocoaWindowMoveLoop is
  // created.
  NSPoint initial_mouse_in_screen_;

  // Pointer to a stack variable holding the exit reason.
  raw_ptr<LoopExitReason> exit_reason_ref_ = nullptr;
  base::OnceClosure quit_closure_;

  std::unique_ptr<gfx::ScopedCocoaDisableScreenUpdates> screen_disabler_;

  // WeakPtrFactory for event monitor safety.
  base::WeakPtrFactory<CocoaWindowMoveLoop> weak_factory_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_
