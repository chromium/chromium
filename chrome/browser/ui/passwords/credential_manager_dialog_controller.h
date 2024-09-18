// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/passwords/password_base_dialog_controller.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "ui/gfx/range/range.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// An interface used by the password dialog (the account chooser) for setting
// and retrieving the state.
class CredentialManagerDialogController : public PasswordBaseDialogController {
 public:
  using FormsVector =
      std::vector<std::unique_ptr<password_manager::PasswordForm>>;

  // Returns forms from the password database for the current site.
  virtual const FormsVector& GetLocalForms() const = 0;

  // Returns a title of the account chooser.
  virtual std::u16string GetAccountChooserTitle() const = 0;

  // Whether the account chooser should display the "Sign in" button.
  virtual bool ShouldShowSignInButton() const = 0;

  // Returns the title for the autosignin first run dialog.
  virtual std::u16string GetAutoSigninPromoTitle() const = 0;

  // Returns a text of the auto signin first run promo.
  virtual std::u16string GetAutoSigninText() const = 0;

  // Returns true if the footer about Google Account should be shown.
  virtual bool ShouldShowFooter() const = 0;

  // Called when the user chooses a credential.
  virtual void OnChooseCredentials(
      const password_manager::PasswordForm& password_form,
      password_manager::CredentialType credential_type) = 0;

  // Called when the user clicks "Sign in" in the account chooser.
  virtual void OnSignInClicked() = 0;

  // Called when user clicks OK in the auto signin first run promo.
  virtual void OnAutoSigninOK() = 0;

  // Called when user disables the auto signin setting.
  virtual void OnAutoSigninTurnOff() = 0;

  // Called when the dialog was closed.
  virtual void OnCloseDialog() = 0;

 protected:
  ~CredentialManagerDialogController() override = default;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_H_
