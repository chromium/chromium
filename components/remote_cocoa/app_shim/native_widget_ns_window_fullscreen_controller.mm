// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/native_widget_ns_window_fullscreen_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/cocoa/nswindow_test_util.h"

namespace remote_cocoa {

namespace {

bool IsFakeForTesting() {
  return ui::NSWindowFakedForTesting::IsEnabled();
}

}  // namespace

NativeWidgetNSWindowFullscreenController::
    NativeWidgetNSWindowFullscreenController(Client* client)
    : client_(client) {}

NativeWidgetNSWindowFullscreenController::
    ~NativeWidgetNSWindowFullscreenController() = default;

void NativeWidgetNSWindowFullscreenController::EnterFullscreen(
    int64_t target_display_id) {
  if (IsFakeForTesting()) {
    if (state_ == State::kWindowed) {
      state_ = State::kEnterFullscreenTransition;
      client_->FullscreenControllerTransitionStart(true);

      windowed_frame_ = client_->FullscreenControllerGetFrame();
      const gfx::Rect kFakeFullscreenRect(0, 0, 1024, 768);
      client_->FullscreenControllerSetFrame(kFakeFullscreenRect,
                                            /*animate=*/false,
                                            base::DoNothing());
      state_ = State::kFullscreen;
      client_->FullscreenControllerTransitionComplete(true);
    }
    return;
  }

  if (state_ == State::kFullscreen) {
    // Early-out for no-ops.
    if (target_display_id == display::kInvalidDisplayId ||
        target_display_id == client_->FullscreenControllerGetDisplayId()) {
      return;
    }
  } else if (state_ == State::kWindowed) {
    windowed_frame_ = client_->FullscreenControllerGetFrame();
  }

  // If we are starting a new transition, then notify `client_`.
  if (!IsInFullscreenTransition()) {
    client_->FullscreenControllerTransitionStart(true);
  }

  pending_state_ = PendingState();
  pending_state_->is_fullscreen = true;
  pending_state_->display_id = target_display_id;
  HandlePendingState();
  DCHECK(IsInFullscreenTransition());
}

void NativeWidgetNSWindowFullscreenController::ExitFullscreen() {
  if (IsFakeForTesting()) {
    if (state_ == State::kFullscreen) {
      state_ = State::kExitFullscreenTransition;
      client_->FullscreenControllerTransitionStart(false);
      client_->FullscreenControllerSetFrame(windowed_frame_.value(),
                                            /*animate=*/false,
                                            base::DoNothing());
      state_ = State::kWindowed;
      client_->FullscreenControllerTransitionComplete(false);
    }
    return;
  }

  // Early-out for no-ops.
  if (state_ == State::kWindowed)
    return;

  // If we are starting a new transition, then notify `client_`.
  if (!IsInFullscreenTransition())
    client_->FullscreenControllerTransitionStart(false);

  pending_state_ = PendingState();
  pending_state_->is_fullscreen = false;
  HandlePendingState();
  DCHECK(IsInFullscreenTransition());
}

void NativeWidgetNSWindowFullscreenController::
    MoveToTargetDisplayThenToggleFullscreen(int64_t target_display_id) {
  DCHECK_EQ(state_, State::kWindowedMovingToFullscreenTarget);

  gfx::Rect display_frame =
      client_->FullscreenControllerGetFrameForDisplay(target_display_id);
  if (!display_frame.IsEmpty()) {
    DCHECK(windowed_frame_);
    restore_windowed_frame_ = true;
    SetStateAndCancelPostedTasks(State::kEnterFullscreenTransition);
    client_->FullscreenControllerSetFrame(
        display_frame, /*animate=*/true,
        base::BindOnce(
            &NativeWidgetNSWindowFullscreenController::ToggleFullscreen,
            weak_factory_.GetWeakPtr()));
  }
}

void NativeWidgetNSWindowFullscreenController::RestoreWindowedFrame() {
  DCHECK_EQ(state_, State::kWindowedRestoringOriginalFrame);
  DCHECK(restore_windowed_frame_);
  DCHECK(windowed_frame_);
  client_->FullscreenControllerSetFrame(
      windowed_frame_.value(),
      /*animate=*/true,
      base::BindOnce(
          &NativeWidgetNSWindowFullscreenController::OnWindowedFrameRestored,
          weak_factory_.GetWeakPtr()));
}

void NativeWidgetNSWindowFullscreenController::OnWindowedFrameRestored() {
  restore_windowed_frame_ = false;

  SetStateAndCancelPostedTasks(State::kWindowed);
  HandlePendingState();
  if (!IsInFullscreenTransition()) {
    client_->FullscreenControllerTransitionComplete(
        /*is_fullscreen=*/false);
  }
}

void NativeWidgetNSWindowFullscreenController::ToggleFullscreen() {
  // Note that OnWindowWillEnterFullscreen or OnWindowWillExitFullscreen will
  // be called within the below call.
  client_->FullscreenControllerToggleFullscreen();
}

bool NativeWidgetNSWindowFullscreenController::CanResize() const {
  // Don't modify the size constraints or fullscreen collection behavior while
  // in fullscreen or during a transition. OnFullscreenTransitionComplete will
  // reset these after leaving fullscreen.
  return state_ == State::kWindowed;
}

void NativeWidgetNSWindowFullscreenController::SetStateAndCancelPostedTasks(
    State new_state) {
  weak_factory_.InvalidateWeakPtrs();
  state_ = new_state;
}

void NativeWidgetNSWindowFullscreenController::OnWindowWantsToClose() {
  if (state_ == State::kEnterFullscreenTransition ||
      state_ == State::kExitFullscreenTransition) {
    has_deferred_window_close_ = true;
  }
}

void NativeWidgetNSWindowFullscreenController::OnWindowWillClose() {
  // If a window closes while in a fullscreen transition, then the window will
  // hang in a zombie-like state.
  // https://crbug.com/945237
  if (state_ != State::kWindowed && state_ != State::kFullscreen) {
    DLOG(ERROR) << "-[NSWindow close] while in fullscreen transition will "
                   "trigger zombie windows.";
  }
}

void NativeWidgetNSWindowFullscreenController::OnWindowWillEnterFullscreen() {
  if (state_ == State::kWindowed) {
    windowed_frame_ = client_->FullscreenControllerGetFrame();
  }

  // If we are starting a new transition, then notify `client_`.
  if (!IsInFullscreenTransition()) {
    client_->FullscreenControllerTransitionStart(true);
  }

  SetStateAndCancelPostedTasks(State::kEnterFullscreenTransition);
  DCHECK(IsInFullscreenTransition());
}

void NativeWidgetNSWindowFullscreenController::OnWindowDidEnterFullscreen() {
  if (HandleDeferredClose())
    return;
  if (state_ == State::kExitFullscreenTransition) {
    // If transitioning out of fullscreen failed, then just remain in
    // fullscreen. Note that `pending_state_` could have been left present for a
    // fullscreen-to-fullscreen-on-another-display transition. If it looks like
    // we are in that situation, reset `pending_state_`.
    if (pending_state_ && pending_state_->is_fullscreen)
      pending_state_.reset();
  }
  SetStateAndCancelPostedTasks(State::kFullscreen);
  HandlePendingState();
  if (!IsInFullscreenTransition()) {
    client_->FullscreenControllerTransitionComplete(
        /*target_fullscreen_state=*/true);
  }
}

void NativeWidgetNSWindowFullscreenController::OnWindowWillExitFullscreen() {
  // If we are starting a new transition, then notify `client_`.
  if (!IsInFullscreenTransition())
    client_->FullscreenControllerTransitionStart(false);

  SetStateAndCancelPostedTasks(State::kExitFullscreenTransition);
  DCHECK(IsInFullscreenTransition());
}

void NativeWidgetNSWindowFullscreenController::OnWindowDidExitFullscreen() {
  if (HandleDeferredClose())
    return;
  SetStateAndCancelPostedTasks(State::kWindowed);
  HandlePendingState();
  if (!IsInFullscreenTransition()) {
    client_->FullscreenControllerTransitionComplete(
        /*is_fullscreen=*/false);
  }
}

void NativeWidgetNSWindowFullscreenController::HandlePendingState() {
  // If in kWindowed or kFullscreen, then consume `pending_state_`.
  switch (state_) {
    case State::kClosed:
      pending_state_.reset();
      return;
    case State::kWindowed:
      if (pending_state_ && pending_state_->is_fullscreen) {
        if (pending_state_->display_id != display::kInvalidDisplayId &&
            pending_state_->display_id !=
                client_->FullscreenControllerGetDisplayId()) {
          // Handle entering fullscreen on another specified display.
          SetStateAndCancelPostedTasks(
              State::kWindowedMovingToFullscreenTarget);
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&NativeWidgetNSWindowFullscreenController::
                                 MoveToTargetDisplayThenToggleFullscreen,
                             weak_factory_.GetWeakPtr(),
                             pending_state_->display_id));
        } else {
          // Handle entering fullscreen on the default display.
          SetStateAndCancelPostedTasks(State::kEnterFullscreenTransition);
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &NativeWidgetNSWindowFullscreenController::ToggleFullscreen,
                  weak_factory_.GetWeakPtr()));
        }
      } else if (restore_windowed_frame_) {
        // Handle returning to the kWindowed state after having been fullscreen
        // and having called setFrame during some transition. It is necessary
        // to restore the original frame prior to having entered fullscreen.
        SetStateAndCancelPostedTasks(State::kWindowedRestoringOriginalFrame);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &NativeWidgetNSWindowFullscreenController::RestoreWindowedFrame,
                weak_factory_.GetWeakPtr()));
      }
      // Always reset `pending_state_` when handling kWindowed state.
      pending_state_.reset();
      return;
    case State::kFullscreen:
      if (pending_state_) {
        if (pending_state_->is_fullscreen) {
          // If `pending_state_` is a no-op, then reset it.
          if (pending_state_->display_id == display::kInvalidDisplayId ||
              pending_state_->display_id ==
                  client_->FullscreenControllerGetDisplayId()) {
            pending_state_.reset();
            return;
          }
          // Leave `pending_state_` in place. It will be consumed when we
          // come through here again via OnWindowDidExitFullscreen (or
          // via OnWindowDidEnterFullscreen, if we fail to exit fullscreen).
        } else {
          pending_state_.reset();
        }
        SetStateAndCancelPostedTasks(State::kExitFullscreenTransition);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &NativeWidgetNSWindowFullscreenController::ToggleFullscreen,
                weak_factory_.GetWeakPtr()));
      }
      return;
    default:
      // Leave `pending_state_` unchanged. It will be re-examined when our
      // transition completes.
      break;
  }
}

bool NativeWidgetNSWindowFullscreenController::HandleDeferredClose() {
  CHECK_NE(state_, State::kClosed);
  if (has_deferred_window_close_) {
    SetStateAndCancelPostedTasks(State::kClosed);
    // Note that `this` may be deleted by the below call.
    client_->FullscreenControllerCloseWindow();
    return true;
  }
  return false;
}

bool NativeWidgetNSWindowFullscreenController::GetTargetFullscreenState()
    const {
  if (pending_state_)
    return pending_state_->is_fullscreen;
  switch (state_) {
    case State::kWindowed:
    case State::kWindowedRestoringOriginalFrame:
    case State::kExitFullscreenTransition:
    case State::kClosed:
      return false;
    case State::kWindowedMovingToFullscreenTarget:
    case State::kEnterFullscreenTransition:
    case State::kFullscreen:
      return true;
  }
}

bool NativeWidgetNSWindowFullscreenController::IsInFullscreenTransition()
    const {
  switch (state_) {
    case State::kWindowed:
    case State::kFullscreen:
    case State::kClosed:
      return false;
    default:
      return true;
  }
}

}  // namespace remote_cocoa
