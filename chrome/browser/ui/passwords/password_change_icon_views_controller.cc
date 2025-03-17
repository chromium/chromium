// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_icon_views_controller.h"

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "content/public/browser/web_contents.h"

PasswordChangeIconViewsController::PasswordChangeIconViewsController(
    base::RepeatingClosure update_ui_callback,
    base::RepeatingClosure update_visibility_callback)
    : update_ui_callback_(std::move(update_ui_callback)),
      update_visibility_callback_(std::move(update_visibility_callback)) {}

PasswordChangeIconViewsController::~PasswordChangeIconViewsController() =
    default;

void PasswordChangeIconViewsController::SetPasswordChangeDelegate(
    PasswordChangeDelegate* delegate) {
  if (delegate == password_change_delegate_) {
    return;
  }
  scoped_observation_.Reset();
  password_change_delegate_ = delegate;
  if (password_change_delegate_) {
    OnStateChanged(password_change_delegate_->GetCurrentState());
    scoped_observation_.Observe(password_change_delegate_);
  }
}

PasswordChangeDelegate::State
PasswordChangeIconViewsController::GetCurrentState() const {
  return state_;
}

void PasswordChangeIconViewsController::OnStateChanged(
    PasswordChangeDelegate::State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  update_ui_callback_.Run();
}

void PasswordChangeIconViewsController::OnPasswordChangeStopped(
    PasswordChangeDelegate* delegate) {
  scoped_observation_.Reset();
  password_change_delegate_ = nullptr;
  update_visibility_callback_.Run();
}
