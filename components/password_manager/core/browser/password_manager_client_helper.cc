// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_client_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace password_manager {

namespace {

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
bool IsPrimaryAccountSignIn(const signin::IdentityManager& identity_manager,
                            const std::u16string& username,
                            const std::string& signon_realm) {
  CoreAccountInfo primary_account =
      identity_manager.GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return sync_util::IsGaiaCredentialPage(signon_realm) &&
         !primary_account.IsEmpty() &&
         gaia::AreEmailsSame(base::UTF16ToUTF8(username),
                             primary_account.email);
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace

PasswordManagerClientHelper::PasswordManagerClientHelper(
    PasswordManagerClient* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

PasswordManagerClientHelper::~PasswordManagerClientHelper() = default;

void PasswordManagerClientHelper::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<PasswordForm> form) {
  possible_auto_sign_in_ = std::move(form);
}

void PasswordManagerClientHelper::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<PasswordFormManagerForUI> submitted_manager) {
  const PasswordForm& form = submitted_manager->GetPendingCredentials();
  if (!possible_auto_sign_in_ ||
      possible_auto_sign_in_->username_value != form.username_value ||
      possible_auto_sign_in_->password_value != form.password_value ||
      possible_auto_sign_in_->url != form.url ||
      !ShouldPromptToEnableAutoSignIn()) {
    possible_auto_sign_in_.reset();
  }
  // Check if it is necessary to prompt user to enable auto sign-in.
  if (possible_auto_sign_in_) {
    delegate_->PromptUserToEnableAutosignin();
  } else if (ShouldPromptToMovePasswordToAccount(*submitted_manager)) {
    delegate_->PromptUserToMovePasswordToAccount(std::move(submitted_manager));
  }
}

void PasswordManagerClientHelper::OnCredentialsChosen(
    PasswordManagerClient::CredentialsCallback callback,
    bool one_local_credential,
    const PasswordForm* form) {
  std::move(callback).Run(form);
  // If a site gets back a credential some navigations are likely to occur. They
  // shouldn't trigger the autofill password manager.
  if (form) {
    delegate_->GetPasswordManager()->DropFormManagers();
  }
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
  return password_bubble_experiment::
             ShouldShowAutoSignInPromptFirstRunExperience(
                 delegate_->GetPrefs()) &&
         delegate_->IsAutoSignInEnabled() && !delegate_->IsOffTheRecord();
}

bool PasswordManagerClientHelper::ShouldPromptToMovePasswordToAccount(
    const PasswordFormManagerForUI& submitted_manager) const {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  PasswordFeatureManager* feature_manager =
      delegate_->GetPasswordFeatureManager();
  if (!feature_manager->ShouldShowAccountStorageBubbleUi()) {
    return false;
  }
  if (!feature_manager->IsOptedInForAccountStorage()) {
    return false;
  }
  if (feature_manager->GetDefaultPasswordStore() ==
      PasswordForm::Store::kProfileStore) {
    return false;
  }
  if (!submitted_manager.IsMovableToAccountStore()) {
    return false;
  }
  if (delegate_->IsOffTheRecord()) {
    return false;
  }
  // It's not useful to store the password for the primary account inside
  // that same account.
  if (IsPrimaryAccountSignIn(
          *delegate_->GetIdentityManager(),
          submitted_manager.GetPendingCredentials().username_value,
          submitted_manager.GetPendingCredentials().signon_realm)) {
    return false;
  }
  return true;
#else
  // On Android and iOS, prompting to move after using a password isn't
  // implemented.
  return false;
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

}  // namespace password_manager
