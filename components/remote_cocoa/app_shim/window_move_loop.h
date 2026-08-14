// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_

#import <Cocoa/Cocoa.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "ui/gfx/mac/scoped_cocoa_disable_screen_updates.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;

// Used by NativeWidgetNSWindowBridge when dragging detached tabs.
class REMOTE_COCOA_APP_SHIM_EXPORT CocoaWindowMoveLoop {
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

  // Updates the baseline window frame and mouse position. Called when the
  // window bounds or size are updated programmatically during the drag (e.g.
  // by TabDragController when moving across displays).
  void SetBaseFrame(const NSRect& new_base_frame,
                    const NSPoint& new_base_mouse);

  const NSRect& base_frame_for_testing() const { return base_frame_; }
  const NSPoint& base_mouse_for_testing() const {
    return base_mouse_in_screen_;
  }
  const NSRect& last_set_frame_for_testing() const { return last_set_frame_; }

 private:
  enum LoopExitReason {
    ENDED_EXTERNALLY,
    MOUSE_UP,
    WINDOW_DESTROYED,
  };

  raw_ptr<NativeWidgetNSWindowBridge> owner_;  // Weak. Owns this.

  // Baseline mouse location for relative drag offset calculations. Re-anchored
  // if the window size changes mid-drag.
  NSPoint base_mouse_in_screen_;

  // Baseline window frame before dragging, or updated baseline frame if the
  // window size changed programmatically during the drag.
  NSRect base_frame_;

  // The last window frame that was explicitly set by this move loop. Used to
  // detect if the window frame was changed programmatically from outside of
  // the move loop (e.g. by TabDragController to fit a new display work area).
  NSRect last_set_frame_;

  // Pointer to a stack variable holding the exit reason.
  raw_ptr<LoopExitReason> exit_reason_ref_ = nullptr;
  base::OnceClosure quit_closure_;

  std::unique_ptr<gfx::ScopedCocoaDisableScreenUpdates> screen_disabler_;

  // WeakPtrFactory for event monitor safety.
  base::WeakPtrFactory<CocoaWindowMoveLoop> weak_factory_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_MOVE_LOOP_H_
