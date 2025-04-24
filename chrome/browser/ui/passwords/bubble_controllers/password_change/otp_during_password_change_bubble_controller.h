// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_OTP_DURING_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_OTP_DURING_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

class PasswordChangeDelegate;

// Controller for OtpDuringPasswordChangeView which is displayed after
// OTP field was detected during the password change flow.
class OtpDuringPasswordChangeBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit OtpDuringPasswordChangeBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);

  ~OtpDuringPasswordChangeBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  std::u16string GetBody() const;
  std::u16string GetAcceptButtonText() const;

  // Opens a tab where password change is ongoing.
  void FixManually();

  // Marks password change flow as completed.
  void FinishPasswordChange();

  void NavigateToPasswordChangeSettings();

 private:
  base::WeakPtr<PasswordChangeDelegate> password_change_delegate_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_OTP_DURING_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_
