// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/user_adding_screen_input_methods_controller.h"

namespace ash {

UserAddingScreenInputMethodsController::
    UserAddingScreenInputMethodsController() = default;

UserAddingScreenInputMethodsController::
    ~UserAddingScreenInputMethodsController() = default;

void UserAddingScreenInputMethodsController::OnScreenShown(
    bool has_active_session) {
  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();
  saved_ime_state_ = imm->GetActiveIMEState();
  imm->SetState(saved_ime_state_->Clone());
  imm->GetActiveIMEState()->DisableNonLockScreenLayouts();
  imm->GetActiveIMEState()->SetUIStyle(
      has_active_session
          ? input_method::InputMethodManager::UIStyle::kSecondaryLogin
          : input_method::InputMethodManager::UIStyle::kLogin);
}

void UserAddingScreenInputMethodsController::OnScreenHidden(
    bool active_user_changed) {
  if (!active_user_changed) {
    input_method::InputMethodManager::Get()->SetState(saved_ime_state_);
  }

  saved_ime_state_.reset();
}

}  // namespace ash
