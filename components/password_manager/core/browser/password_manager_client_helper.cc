// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_client_helper.h"

#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

PasswordManagerClientHelper::PasswordManagerClientHelper(
    PasswordManagerClient* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

PasswordManagerClientHelper::~PasswordManagerClientHelper() {}

void PasswordManagerClientHelper::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {
  possible_auto_sign_in_ = std::move(form);
}

void PasswordManagerClientHelper::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {
  if (!possible_auto_sign_in_)
    return;

  if (possible_auto_sign_in_->username_value == form.username_value &&
      possible_auto_sign_in_->password_value == form.password_value &&
      possible_auto_sign_in_->origin == form.origin) {
    // Check if it is necessary to prompt user to enable auto sign-in.
    if (ShouldPromptToEnableAutoSignIn()) {
      delegate_->PromptUserToEnableAutosignin();
    }
  }
  possible_auto_sign_in_.reset();
}

void PasswordManagerClientHelper::OnCredentialsChosen(
    const PasswordManagerClient::CredentialsCallback& callback,
    bool one_local_credential,
    const autofill::PasswordForm* form) {
  callback.Run(form);
  // If a site gets back a credential some navigations are likely to occur. They
  // shouldn't trigger the autofill password manager.
  if (form)
    delegate_->GetPasswordManager()->DropFormManagers();
  if (form && one_local_credential) {
    if (ShouldPromptToEnableAutoSignIn()) {
      delegate_->PromptUserToEnableAutosignin();
    }
  }
}

void PasswordManagerClientHelper::NotifyStorePasswordCalled() {
  // If a site stores a credential the autofill password manager shouldn't kick
  // in.
  delegate_->GetPasswordManager()->NotifyStorePasswordCalled();
}

void PasswordManagerClientHelper::NotifyUserAutoSignin() {
  // If a site gets back a credential some navigations are likely to occur. They
  // shouldn't trigger the autofill password manager.
  delegate_->GetPasswordManager()->DropFormManagers();
}

bool PasswordManagerClientHelper::ShouldPromptToEnableAutoSignIn() const {
  if (!password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          delegate_->GetPrefs()) ||
      !delegate_->GetPrefs()->GetBoolean(
          password_manager::prefs::kCredentialsEnableAutosignin) ||
      delegate_->IsIncognito())
    return false;
  return true;
}

}  // namespace password_manager
