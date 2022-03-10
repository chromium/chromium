// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_FULLSCREEN_CONTROLLER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_FULLSCREEN_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remote_cocoa {

class NativeWidgetNSWindowBridge;

class REMOTE_COCOA_APP_SHIM_EXPORT NativeWidgetNSWindowFullscreenController {
 public:
  explicit NativeWidgetNSWindowFullscreenController(
      NativeWidgetNSWindowBridge* bridge);
  NativeWidgetNSWindowFullscreenController(
      const NativeWidgetNSWindowFullscreenController&) = delete;
  NativeWidgetNSWindowFullscreenController& operator=(
      const NativeWidgetNSWindowFullscreenController&) = delete;
  ~NativeWidgetNSWindowFullscreenController();

  // Called by NativeWidget::SetFullscreen.
  void EnterFullscreen();
  void ExitFullscreen();

  // Called from NativeWidgetNSWindowBridge:CloseWindow, indicating that the
  // window has been requested to be closed. If a transition is in progress,
  // then the close will be deferred until after the transition completes.
  void OnWindowWantsToClose();

  // Return true if an active transition has caused closing of the window to be
  // deferred.
  bool HasDeferredWindowClose() const { return has_deferred_window_close_; }

  // Called by NativeWidgetNSWindowBridge::OnWindowWillClose.
  void OnWindowWillClose();

  // Called by -[NSWindowDelegate windowWill/DidEnter/ExitFullScreen:].
  void OnWindowWillEnterFullscreen();
  void OnWindowDidEnterFullscreen();
  void OnWindowWillExitFullscreen();
  void OnWindowDidExitFullscreen();

  // Return false unless the state is kWindowed or kFullscreen.
  bool IsInFullscreenTransition() const;

  // Return true if the window can be resized. The window cannot be resized
  // while fullscreen or during a transition.
  bool CanResize() const;

  // Return the fullscreen state that will be arrived at when all transition
  // is done.
  bool GetTargetFullscreenState() const;

 private:
  enum class State {
    // In windowed mode.
    kWindowed,
    // In transition to enter fullscreen mode. This encompases the following
    // states:
    // - From the kWindowed state, a task for ToggleFullscreen has been
    //   posted.
    // - OnWindowWillEnterFullscreen has been called (either as a result of
    //   ToggleFullscreen, or as a result of user interaction), but neither
    //   OnWindowDidEnterFullscreen nor OnWindowDidExitFullscreen have been
    //   called yet.
    kEnterFullscreenTransition,
    // In fullscreen mode.
    kFullscreen,
    // In transition to exit fullscreen mode. This encompases the following
    // states:
    // - From the kFullscreen state, a task for ToggleFullscreen has been
    //   posted.
    // - OnWindowWillExitFullscreen has been called (either as a result of
    //   ToggleFullscreen, or as a result of user interaction), but neither
    //   OnWindowDidExitFullscreen nor OnWindowDidEnterFullscreen have been
    //   called yet.
    kExitFullscreenTransition,
    // The window has been closed.
    kClosed,
  };
  struct PendingState {
    bool is_fullscreen = false;
  };

  // Helper function wrapping -[NSWindow toggleFullscreen:].
  void ToggleFullscreen();

  // If not currently in transition, consume `pending_state_` and start a
  // transition to the state it specifies.
  void HandlePendingState();

  // If there exists a deferred close, then close the window, set the
  // current state to kClosed, and return true.
  bool HandleDeferredClose();

  // Set `state` to `new_state`, and invalidate any posted tasks. Posted tasks
  // exist to transition from the current state to a new state, and so if the
  // current state changes, then those tasks are no longer applicable.
  void SetStateAndCancelPostedTasks(State new_state);

  State state_ = State::kWindowed;

  // If a call to EnterFullscreen or ExitFullscreen happens during a
  // transition, then that final requested state is stored in `pending_state_`.
  absl::optional<PendingState> pending_state_;

  // Trying to close an NSWindow during a fullscreen transition will cause the
  // window to lock up. Use this to track if CloseWindow was called during a
  // fullscreen transition, to defer the -[NSWindow close] call until the
  // transition is complete.
  // https://crbug.com/945237
  bool has_deferred_window_close_ = false;

  // Weak, owns `this`.
  const raw_ptr<NativeWidgetNSWindowBridge> window_bridge_;
  base::WeakPtrFactory<NativeWidgetNSWindowFullscreenController> weak_factory_{
      this};
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_FULLSCREEN_CONTROLLER_H_
