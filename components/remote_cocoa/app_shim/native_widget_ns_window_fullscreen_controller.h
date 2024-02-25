// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_FULLSCREEN_CONTROLLER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_FULLSCREEN_CONTROLLER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace remote_cocoa {

class NativeWidgetNSWindowBridge;

class REMOTE_COCOA_APP_SHIM_EXPORT NativeWidgetNSWindowFullscreenController {
 public:
  class Client {
   public:
    // Called when a transition between fullscreen and windowed (or vice-versa).
    // If `is_target_fullscreen` is true, then the target of the transition is
    // fullscreen.
    virtual void FullscreenControllerTransitionStart(
        bool is_target_fullscreen) = 0;

    // Called when a transition between fullscreen and windowed is complete.
    // If `is_fullscreen` is true, then the window is now fullscreen.
    virtual void FullscreenControllerTransitionComplete(bool is_fullscreen) = 0;

    // Set the window's frame to the specified rectangle. If `animate` is true,
    // then animate the transition. Runs the `completion_callback` callback once
    // the animation is complete, or immediately when `animate` is false.
    virtual void FullscreenControllerSetFrame(
        const gfx::Rect& frame,
        bool animate,
        base::OnceCallback<void()> completion_callback) = 0;

    // Call -[NSWindow toggleFullscreen:].
    virtual void FullscreenControllerToggleFullscreen() = 0;

    // Call -[NSWindow close]. Note that this call may result in the caller
    // being destroyed.
    virtual void FullscreenControllerCloseWindow() = 0;

    // Return the display id for the display that the window is currently on.
    virtual int64_t FullscreenControllerGetDisplayId() const = 0;

    // Return the frame that should be set prior to transitioning to fullscreen
    // on the display specified by `display_id`. If `display_id` is invalid,
    // then return an empty rectangle.
    virtual gfx::Rect FullscreenControllerGetFrameForDisplay(
        int64_t display_id) const = 0;

    // Get the window's current frame.
    virtual gfx::Rect FullscreenControllerGetFrame() const = 0;
  };

  explicit NativeWidgetNSWindowFullscreenController(Client* client);
  NativeWidgetNSWindowFullscreenController(
      const NativeWidgetNSWindowFullscreenController&) = delete;
  NativeWidgetNSWindowFullscreenController& operator=(
      const NativeWidgetNSWindowFullscreenController&) = delete;
  ~NativeWidgetNSWindowFullscreenController();

  // Called by NativeWidget::SetFullscreen.
  void EnterFullscreen(int64_t target_display_id);
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
    // Moving the window to the target display on which it will go fullscreen.
    kWindowedMovingToFullscreenTarget,
    // In transition to enter fullscreen mode. This encompasses the following
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
    // In transition to exit fullscreen mode. This encompasses the following
    // states:
    // - From the kFullscreen state, a task for ToggleFullscreen has been
    //   posted.
    // - OnWindowWillExitFullscreen has been called (either as a result of
    //   ToggleFullscreen, or as a result of user interaction), but neither
    //   OnWindowDidExitFullscreen nor OnWindowDidEnterFullscreen have been
    //   called yet.
    kExitFullscreenTransition,
    // Moving the window back to its original position from before it entered
    // fullscreen.
    kWindowedRestoringOriginalFrame,
    // The window has been closed.
    kClosed,
  };
  struct PendingState {
    bool is_fullscreen = false;
    int64_t display_id = display::kInvalidDisplayId;
  };

  // Move the window to `target_display_id`, and then post a task to go
  // fullscreen.
  void MoveToTargetDisplayThenToggleFullscreen(int64_t target_display_id);

  // Set the window's frame back to `windowed_frame_`, and then return to
  // the kWindowed state.
  void RestoreWindowedFrame();
  // Notifies the client that the fullscreen exit transition has completed after
  // the frame has been restored to its original position.
  void OnWindowedFrameRestored();

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
  std::optional<PendingState> pending_state_;

  // If we call setFrame while in fullscreen transitions, then we will need to
  // restore the original window frame when we return to windowed mode. We save
  // that original frame in `windowed_frame_`, and set set
  // `restore_windowed_frame_` to true if we call setFrame.
  bool restore_windowed_frame_ = false;
  std::optional<gfx::Rect> windowed_frame_;

  // Trying to close an NSWindow during a fullscreen transition will cause the
  // window to lock up. Use this to track if CloseWindow was called during a
  // fullscreen transition, to defer the -[NSWindow close] call until the
  // transition is complete.
  // https://crbug.com/945237
  bool has_deferred_window_close_ = false;

  // Weak, owns `this`.
  const raw_ptr<Client> client_;
  base::WeakPtrFactory<NativeWidgetNSWindowFullscreenController> weak_factory_{
      this};
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_FULLSCREEN_CONTROLLER_H_
