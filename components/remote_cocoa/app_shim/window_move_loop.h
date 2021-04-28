// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;

// Used by NativeWidgetNSWindowBridge when dragging detached tabs.
class CocoaWindowMoveLoop {
 public:
  CocoaWindowMoveLoop(NativeWidgetNSWindowBridge* owner,
                      const NSPoint& initial_mouse_in_screen);
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

  NativeWidgetNSWindowBridge* owner_;  // Weak. Owns this.

  // Initial mouse location at the time before the CocoaWindowMoveLoop is
  // created.
  NSPoint initial_mouse_in_screen_;

  // Pointer to a stack variable holding the exit reason.
  LoopExitReason* exit_reason_ref_ = nullptr;
  base::OnceClosure quit_closure_;

  // WeakPtrFactory for event monitor safety.
  base::WeakPtrFactory<CocoaWindowMoveLoop> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CocoaWindowMoveLoop);
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_
