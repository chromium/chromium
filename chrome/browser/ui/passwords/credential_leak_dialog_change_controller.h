// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CHANGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CHANGE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/credential_leak_dialog_base_controller.h"

// A UI controller responsible for the credential leak change password dialog.
// The dialog contains only one (OK) button asking the user to change the
// password. This dialog is not actionable.
class CredentialLeakDialogChangeController
    : public CredentialLeakDialogBaseController {
 public:
  CredentialLeakDialogChangeController(
      PasswordsLeakDialogDelegate* delegate,
      password_manager::metrics_util::LeakDialogType dialog_type);

  CredentialLeakDialogChangeController(
      const CredentialLeakDialogChangeController&) = delete;
  CredentialLeakDialogChangeController& operator=(
      const CredentialLeakDialogChangeController&) = delete;

  // CredentialLeakDialogController:
  void OnAcceptDialog() override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetDescription() const override;
  std::u16string GetTitle() const override;
  bool ShouldCheckPasswords() const override;
  bool ShouldShowCancelButton() const override;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CHANGE_CONTROLLER_H_
