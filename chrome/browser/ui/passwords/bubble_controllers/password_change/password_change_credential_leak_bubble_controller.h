// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIAL_LEAK_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIAL_LEAK_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

// Controller for the credential leak bubble, which is only displayed when the
// password change is supported.
class PasswordChangeCredentialLeakBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit PasswordChangeCredentialLeakBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);

  ~PasswordChangeCredentialLeakBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  // Get the change password origin to be displayed in UI.
  std::u16string GetDisplayOrigin() const;
  std::u16string GetPrimaryAccountEmail() const;

  void NavigateToPasswordChangeSettings();
  void ChangePassword();
  void Cancel();

 private:
  base::WeakPtr<PasswordChangeDelegate> password_change_delegate_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIAL_LEAK_BUBBLE_CONTROLLER_H_
