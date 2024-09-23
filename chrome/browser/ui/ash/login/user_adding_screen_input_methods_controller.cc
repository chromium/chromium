// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/user_adding_screen_input_methods_controller.h"

#include "components/user_manager/user_manager.h"

namespace ash {

UserAddingScreenInputMethodsController::UserAddingScreenInputMethodsController(
    UserAddingScreen* screen)
    : screen_(screen), active_user_on_show_(nullptr) {
  screen_->AddObserver(this);
}

UserAddingScreenInputMethodsController::
    ~UserAddingScreenInputMethodsController() {
  screen_->RemoveObserver(this);
}

void UserAddingScreenInputMethodsController::OnBeforeUserAddingScreenStarted() {
  active_user_on_show_ = user_manager::UserManager::Get()->GetActiveUser();
  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();
  saved_ime_state_ = imm->GetActiveIMEState();
  imm->SetState(saved_ime_state_->Clone());
  imm->GetActiveIMEState()->DisableNonLockScreenLayouts();
  imm->GetActiveIMEState()->SetUIStyle(
      user_manager::UserManager::Get()->IsUserLoggedIn()
          ? input_method::InputMethodManager::UIStyle::kSecondaryLogin
          : input_method::InputMethodManager::UIStyle::kLogin);
}

void UserAddingScreenInputMethodsController::OnUserAddingFinished() {
  if (user_manager::UserManager::Get()->GetActiveUser() ==
      active_user_on_show_) {
    input_method::InputMethodManager::Get()->SetState(saved_ime_state_);
  }

  saved_ime_state_.reset();
}

}  // namespace ash
