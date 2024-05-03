// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ADD_USERNAME_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ADD_USERNAME_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/common_saved_account_manager_bubble_controller.h"

// This controller provides data and actions for the PasswordAddUsernameView.
class AddUsernameBubbleController
    : public CommonSavedAccountManagerBubbleController {
 public:
  explicit AddUsernameBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      DisplayReason display_reason);
  ~AddUsernameBubbleController() override;

  // Called by the view code when the 'Add username' button is clicked by the
  // user.
  void OnSaveClicked();

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;

 private:
  void ReportInteractions() override;

  // Used to not include this initial form in the confirmation bubble after
  // username is added.
  const password_manager::PasswordForm ininial_pending_password_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ADD_USERNAME_BUBBLE_CONTROLLER_H_
