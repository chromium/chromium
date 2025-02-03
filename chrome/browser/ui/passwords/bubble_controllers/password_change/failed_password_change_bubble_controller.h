// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_FAILED_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_FAILED_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

class PasswordChangeDelegate;

// Controller for FailedPasswordChangeView which is displayed after
// failed password change.
class FailedPasswordChangeBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit FailedPasswordChangeBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);

  ~FailedPasswordChangeBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  std::u16string GetBody() const;
  std::u16string GetAcceptButton() const;

  // Opens a tab where password change is ongoing.
  void FixManually();

  // Marks password change flow as completed.
  void FinishPasswordChange();

  void NavigateToPasswordChangeSettings();

 private:
  base::WeakPtr<PasswordChangeDelegate> password_change_delegate_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_FAILED_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_
