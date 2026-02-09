// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

  CredentialManagerDialogControllerImpl(
      const CredentialManagerDialogControllerImpl&) = delete;
  CredentialManagerDialogControllerImpl& operator=(
      const CredentialManagerDialogControllerImpl&) = delete;

  ~CredentialManagerDialogControllerImpl() override;

  // Pop up the account chooser dialog.
  void ShowAccountChooser(AccountChooserPrompt* dialog, FormsVector locals);

  // Pop up the autosignin first run dialog.
  void ShowAutosigninPrompt(AutoSigninFirstRunPrompt* dialog);

  // CredentialManagerDialogController:
  const FormsVector& GetLocalForms() const override;
  std::u16string GetAccountChooserTitle() const override;
  bool IsShowingAccountChooser() const override;
  bool ShouldShowSignInButton() const override;
  std::u16string GetAutoSigninPromoTitle() const override;
  std::u16string GetAutoSigninText() const override;
  bool ShouldShowFooter() const override;
  void OnChooseCredentials(
      const password_manager::PasswordForm& password_form,
      password_manager::CredentialType credential_type) override;
  void OnSignInClicked() override;
  void OnAutoSigninOK() override;
  void OnAutoSigninTurnOff() override;
  void OnCloseDialog() override;

 private:
  // Release |current_dialog_| and close the open dialog.
  void ResetDialog();
  void OnBiometricReauthCompleted(
      password_manager::PasswordForm password_form,
      password_manager::CredentialType credential_type,
      bool result);

  const raw_ptr<Profile> profile_;
  const raw_ptr<PasswordsModelDelegate> delegate_;
  raw_ptr<AccountChooserPrompt> account_chooser_dialog_;
  raw_ptr<AutoSigninFirstRunPrompt> autosignin_dialog_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials_;
  base::WeakPtrFactory<CredentialManagerDialogControllerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_IMPL_H_
