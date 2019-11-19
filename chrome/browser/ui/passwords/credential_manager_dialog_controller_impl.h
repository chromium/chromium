// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"

class AccountChooserPrompt;
class AutoSigninFirstRunPrompt;
class PasswordsModelDelegate;
class Profile;

// A UI controller responsible for the account chooser dialog and autosignin
// first run promo.
class CredentialManagerDialogControllerImpl
    : public CredentialManagerDialogController {
 public:
  CredentialManagerDialogControllerImpl(Profile* profile,
                                        PasswordsModelDelegate* delegate);
  ~CredentialManagerDialogControllerImpl() override;

  // Pop up the account chooser dialog.
  void ShowAccountChooser(AccountChooserPrompt* dialog, FormsVector locals);

  // Pop up the autosignin first run dialog.
  void ShowAutosigninPrompt(AutoSigninFirstRunPrompt* dialog);

  // CredentialManagerDialogController:
  const FormsVector& GetLocalForms() const override;
  base::string16 GetAccoutChooserTitle() const override;
  bool IsShowingAccountChooser() const override;
  bool ShouldShowSignInButton() const override;
  base::string16 GetAutoSigninPromoTitle() const override;
  base::string16 GetAutoSigninText() const override;
  bool ShouldShowFooter() const override;
  void OnChooseCredentials(
      const autofill::PasswordForm& password_form,
      password_manager::CredentialType credential_type) override;
  void OnSignInClicked() override;
  void OnAutoSigninOK() override;
  void OnAutoSigninTurnOff() override;
  void OnCloseDialog() override;

 private:
  // Release |current_dialog_| and close the open dialog.
  void ResetDialog();

  Profile* const profile_;
  PasswordsModelDelegate* const delegate_;
  AccountChooserPrompt* account_chooser_dialog_;
  AutoSigninFirstRunPrompt* autosignin_dialog_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerDialogControllerImpl);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_IMPL_H_
