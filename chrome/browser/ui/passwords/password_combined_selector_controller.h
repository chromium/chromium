// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_COMBINED_SELECTOR_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_COMBINED_SELECTOR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "chrome/browser/ui/passwords/password_base_dialog_controller.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/origin.h"

namespace password_manager {
struct PasswordForm;
}

// Base controller interface for PasswordCombinedSelectorView.
class PasswordCombinedSelectorController : public PasswordBaseDialogController {
 public:
  using FormsVector =
      std::vector<std::unique_ptr<password_manager::PasswordForm>>;

  PasswordCombinedSelectorController() = default;
  ~PasswordCombinedSelectorController() override = default;

  virtual const FormsVector& GetLocalForms() const = 0;
  virtual url::Origin GetOrigin() const = 0;
  virtual void OnChooseCredentials(
      const password_manager::PasswordForm& password_form,
      password_manager::CredentialType credential_type) = 0;
  virtual void OnCloseDialog() = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_COMBINED_SELECTOR_CONTROLLER_H_
