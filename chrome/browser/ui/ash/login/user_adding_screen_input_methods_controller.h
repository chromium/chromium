// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_

#include "base/memory/scoped_refptr.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

// Ensure a set of "Login keyboard layouts" is used at UserAddingScreen.
class UserAddingScreenInputMethodsController {
 public:
  UserAddingScreenInputMethodsController();

  UserAddingScreenInputMethodsController(
      const UserAddingScreenInputMethodsController&) = delete;
  UserAddingScreenInputMethodsController& operator=(
      const UserAddingScreenInputMethodsController&) = delete;

  ~UserAddingScreenInputMethodsController();

  // Saves the layouts in use and switches to the login ones.
  // `has_active_session` tells whether the screen was opened on top of a
  // session rather than from the login screen, which picks the UI style.
  void OnScreenShown(bool has_active_session);

  // Puts the saved layouts back, unless a different user is active now, in
  // which case their own layouts are already in place.
  void OnScreenHidden(bool active_user_changed);

 private:
  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_
