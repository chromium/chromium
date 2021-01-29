// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "url/gurl.h"

class CredentialLeakPrompt;
class PasswordsLeakDialogDelegate;

// A UI controller responsible for the credential leak dialog.
class CredentialLeakDialogControllerImpl
    : public CredentialLeakDialogController {
 public:
  CredentialLeakDialogControllerImpl(
      PasswordsLeakDialogDelegate* delegate,
      password_manager::CredentialLeakType leak_type,
      const GURL& origin);
  ~CredentialLeakDialogControllerImpl() override;

  // Pop up the credential leak dialog.
  void ShowCredentialLeakPrompt(CredentialLeakPrompt* dialog);

  // CredentialLeakDialogController:
  bool IsShowingAccountChooser() const override;
  void OnCancelDialog() override;
  void OnAcceptDialog() override;
  void OnCloseDialog() override;
  base::string16 GetAcceptButtonLabel() const override;
  base::string16 GetCancelButtonLabel() const override;
  base::string16 GetDescription() const override;
  base::string16 GetTitle() const override;
  bool ShouldCheckPasswords() const override;
  bool ShouldShowCancelButton() const override;

 private:
  // Release |credential_leak_dialog_| and close the open dialog.
  void ResetDialog();

  CredentialLeakPrompt* credential_leak_dialog_ = nullptr;
  PasswordsLeakDialogDelegate* delegate_;
  const password_manager::CredentialLeakType leak_type_;
  const GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(CredentialLeakDialogControllerImpl);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_
