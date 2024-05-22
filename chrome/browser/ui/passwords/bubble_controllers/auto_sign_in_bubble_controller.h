// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_AUTO_SIGN_IN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_AUTO_SIGN_IN_BUBBLE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/password_form.h"

class PasswordsModelDelegate;

// This controller provides data and actions for the AutoSignInView.
class AutoSignInBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit AutoSignInBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~AutoSignInBubbleController() override;

  // Called by the view code when the auto-sign-in toast is about to close due
  // to timeout.
  void OnAutoSignInToastTimeout();

  const password_manager::PasswordForm& pending_password() const {
    return pending_password_;
  }

 private:
  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  password_manager::PasswordForm pending_password_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_AUTO_SIGN_IN_BUBBLE_CONTROLLER_H_
