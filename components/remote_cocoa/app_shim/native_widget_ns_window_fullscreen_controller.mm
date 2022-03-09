// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/native_widget_ns_window_fullscreen_controller.h"

#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"

namespace remote_cocoa {

NativeWidgetNSWindowFullscreenController::
    NativeWidgetNSWindowFullscreenController(
        NativeWidgetNSWindowBridge* window_bridge)
    : window_bridge_(window_bridge) {}

NativeWidgetNSWindowFullscreenController::
    ~NativeWidgetNSWindowFullscreenController() {}

void NativeWidgetNSWindowFullscreenController::EnterFullscreen() {
  if (state_ == State::kFullscreen)
    return;
  const bool notify_transition_start = state_ == State::kWindowed;
  pending_state_ = PendingState();
  pending_state_->is_fullscreen = true;
  HandlePendingState();
  if (notify_transition_start) {
    DCHECK_NE(state_, State::kWindowed);
    window_bridge_->OnFullscreenTransitionStart(true);
  }
}

void NativeWidgetNSWindowFullscreenController::ExitFullscreen() {
  if (state_ == State::kWindowed)
    return;
  const bool notify_transition_start = state_ == State::kFullscreen;
  pending_state_ = PendingState();
  pending_state_->is_fullscreen = false;
  HandlePendingState();
  if (notify_transition_start) {
    DCHECK_NE(state_, State::kFullscreen);
    window_bridge_->OnFullscreenTransitionStart(false);
  }
}

void NativeWidgetNSWindowFullscreenController::ToggleFullscreen() {
  // Suppress synchronous CA transactions during AppKit fullscreen transition
  // since there is no need for updates during such transition.
  // Re-layout and re-paint will be done after the transition. See
  // https://crbug.com/875707 for potiential problems if we don't suppress.
  // `ca_transaction_sync_suppressed_` will be reset to false when the next
  // frame comes in.
  window_bridge_->ca_transaction_sync_suppressed_ = true;

  // Note that OnWindowWillEnterFullscreen or OnWindowWillExitFullscreen will
  // be called within the below call.
  [window_bridge_->window_ toggleFullScreen:nil];
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
  // If OnWindowWillEnterFullscreen is called from the kWindowed state, then
  // this was not triggered by EnterFullscreen (and is not being called from
  // inside ToggleFullscreen). Therefore, we need to notify `window_bridge_`
  // that a transition is starting, and suppress CATranscation sync.
  const bool notify_transition_start = state_ == State::kWindowed;
  SetStateAndCancelPostedTasks(State::kEnterFullscreenTransition);
  if (notify_transition_start) {
    window_bridge_->ca_transaction_sync_suppressed_ = true;
    window_bridge_->OnFullscreenTransitionStart(
        /*target_fullscreen_state=*/true);
  }
}

void NativeWidgetNSWindowFullscreenController::OnWindowDidEnterFullscreen() {
  if (HandleDeferredClose())
    return;
  SetStateAndCancelPostedTasks(State::kFullscreen);
  HandlePendingState();
  if (!IsInFullscreenTransition()) {
    window_bridge_->OnFullscreenTransitionComplete(
        /*target_fullscreen_state=*/true);
  }
}

void NativeWidgetNSWindowFullscreenController::OnWindowWillExitFullscreen() {
  // See notes in OnWindowWillEnterFullscreen.
  const bool notify_transition_start = state_ == State::kFullscreen;
  SetStateAndCancelPostedTasks(State::kExitFullscreenTransition);
  if (notify_transition_start) {
    window_bridge_->ca_transaction_sync_suppressed_ = true;
    window_bridge_->OnFullscreenTransitionStart(
        /*target_fullscreen_state=*/false);
  }
}

void NativeWidgetNSWindowFullscreenController::OnWindowDidExitFullscreen() {
  if (HandleDeferredClose())
    return;
  SetStateAndCancelPostedTasks(State::kWindowed);
  HandlePendingState();
  if (!IsInFullscreenTransition()) {
    window_bridge_->OnFullscreenTransitionComplete(
        /*actual_fullscreen_state=*/false);
  }
}

void NativeWidgetNSWindowFullscreenController::HandlePendingState() {
  // Early-out for no-ops.
  if (!pending_state_)
    return;

  // If in kWindowed or kFullscreen, then consume `pending_state_`.
  switch (state_) {
    case State::kClosed:
      pending_state_.reset();
      return;
    case State::kWindowed:
      if (!pending_state_->is_fullscreen) {
        pending_state_.reset();
        return;
      }
      pending_state_.reset();
      SetStateAndCancelPostedTasks(State::kEnterFullscreenTransition);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &NativeWidgetNSWindowFullscreenController::ToggleFullscreen,
              weak_factory_.GetWeakPtr()));
      return;
    case State::kFullscreen:
      if (pending_state_->is_fullscreen) {
        pending_state_.reset();
        return;
      }
      pending_state_.reset();
      SetStateAndCancelPostedTasks(State::kExitFullscreenTransition);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &NativeWidgetNSWindowFullscreenController::ToggleFullscreen,
              weak_factory_.GetWeakPtr()));
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
    // Note that `this` may be deleted by the below call to `close`.
    [window_bridge_->window_ close];
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
    case State::kExitFullscreenTransition:
    case State::kClosed:
      return false;
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
      return true;
    default:
      return false;
  }
}

}  // namespace remote_cocoa
