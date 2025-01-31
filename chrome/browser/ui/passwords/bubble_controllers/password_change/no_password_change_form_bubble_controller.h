// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_NO_PASSWORD_CHANGE_FORM_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_NO_PASSWORD_CHANGE_FORM_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

class PasswordChangeDelegate;

// Controller for NoPasswordChangeFormView which is displayed on timeout.
class NoPasswordChangeFormBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit NoPasswordChangeFormBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);

  ~NoPasswordChangeFormBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  void Restart();
  void Cancel();

  std::u16string GetBody() const;
  std::u16string GetAcceptButton() const;

 private:
  base::WeakPtr<PasswordChangeDelegate> password_change_delegate_;
  // Dismissal reason for this bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_NO_PASSWORD_CHANGE_FORM_BUBBLE_CONTROLLER_H_
