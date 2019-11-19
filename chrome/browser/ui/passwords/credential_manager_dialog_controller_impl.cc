// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_manager_dialog_controller_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "ui/base/l10n/l10n_util.h"

CredentialManagerDialogControllerImpl::CredentialManagerDialogControllerImpl(
    Profile* profile,
    PasswordsModelDelegate* delegate)
    : profile_(profile),
      delegate_(delegate),
      account_chooser_dialog_(nullptr),
      autosignin_dialog_(nullptr) {}

CredentialManagerDialogControllerImpl::
    ~CredentialManagerDialogControllerImpl() {
  ResetDialog();
}

void CredentialManagerDialogControllerImpl::ShowAccountChooser(
    AccountChooserPrompt* dialog,
    std::vector<std::unique_ptr<autofill::PasswordForm>> locals) {
  DCHECK(!account_chooser_dialog_);
  DCHECK(!autosignin_dialog_);
  DCHECK(dialog);
  local_credentials_.swap(locals);
  account_chooser_dialog_ = dialog;
  account_chooser_dialog_->ShowAccountChooser();
}

void CredentialManagerDialogControllerImpl::ShowAutosigninPrompt(
    AutoSigninFirstRunPrompt* dialog) {
  DCHECK(!account_chooser_dialog_);
  DCHECK(!autosignin_dialog_);
  DCHECK(dialog);
  autosignin_dialog_ = dialog;
  autosignin_dialog_->ShowAutoSigninPrompt();
}

bool CredentialManagerDialogControllerImpl::IsShowingAccountChooser() const {
  return !!account_chooser_dialog_;
}

const CredentialManagerDialogController::FormsVector&
CredentialManagerDialogControllerImpl::GetLocalForms() const {
  return local_credentials_;
}

base::string16 CredentialManagerDialogControllerImpl::GetAccoutChooserTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_TITLE);
}

bool CredentialManagerDialogControllerImpl::ShouldShowSignInButton() const {
  return local_credentials_.size() == 1;
}

base::string16 CredentialManagerDialogControllerImpl::GetAutoSigninPromoTitle()
    const {
  int message_id = IsSyncingAutosignSetting(profile_)
                       ? IDS_AUTO_SIGNIN_FIRST_RUN_TITLE_MANY_DEVICES
                       : IDS_AUTO_SIGNIN_FIRST_RUN_TITLE_LOCAL_DEVICE;
  return l10n_util::GetStringUTF16(message_id);
}

base::string16 CredentialManagerDialogControllerImpl::GetAutoSigninText()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTO_SIGNIN_FIRST_RUN_TEXT,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TITLE_BRAND));
}

bool CredentialManagerDialogControllerImpl::ShouldShowFooter() const {
  const syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return password_bubble_experiment::IsSmartLockUser(sync_service);
}

void CredentialManagerDialogControllerImpl::OnChooseCredentials(
    const autofill::PasswordForm& password_form,
    password_manager::CredentialType credential_type) {
  if (local_credentials_.size() == 1) {
    password_manager::metrics_util::LogAccountChooserUserActionOneAccount(
        password_manager::metrics_util::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN);
  } else {
    password_manager::metrics_util::LogAccountChooserUserActionManyAccounts(
        password_manager::metrics_util::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN);
  }
  ResetDialog();
  delegate_->ChooseCredential(password_form, credential_type);
}

void CredentialManagerDialogControllerImpl::OnSignInClicked() {
  DCHECK_EQ(1u, local_credentials_.size());
  password_manager::metrics_util::LogAccountChooserUserActionOneAccount(
      password_manager::metrics_util::ACCOUNT_CHOOSER_SIGN_IN);
  ResetDialog();
  delegate_->ChooseCredential(
      *local_credentials_[0],
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

void CredentialManagerDialogControllerImpl::OnAutoSigninOK() {
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      profile_->GetPrefs());
  password_manager::metrics_util::LogAutoSigninPromoUserAction(
      password_manager::metrics_util::AUTO_SIGNIN_OK_GOT_IT);
  ResetDialog();
  OnCloseDialog();
}

void CredentialManagerDialogControllerImpl::OnAutoSigninTurnOff() {
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      profile_->GetPrefs());
  password_manager::metrics_util::LogAutoSigninPromoUserAction(
      password_manager::metrics_util::AUTO_SIGNIN_TURN_OFF);
  ResetDialog();
  OnCloseDialog();
}

void CredentialManagerDialogControllerImpl::OnCloseDialog() {
  if (account_chooser_dialog_) {
    if (local_credentials_.size() == 1) {
      password_manager::metrics_util::LogAccountChooserUserActionOneAccount(
          password_manager::metrics_util::ACCOUNT_CHOOSER_DISMISSED);
    } else {
      password_manager::metrics_util::LogAccountChooserUserActionManyAccounts(
          password_manager::metrics_util::ACCOUNT_CHOOSER_DISMISSED);
    }
    account_chooser_dialog_ = nullptr;
  }
  if (autosignin_dialog_) {
    password_manager::metrics_util::LogAutoSigninPromoUserAction(
        password_manager::metrics_util::AUTO_SIGNIN_NO_ACTION);
    autosignin_dialog_ = nullptr;
  }
  delegate_->OnDialogHidden();
}

void CredentialManagerDialogControllerImpl::ResetDialog() {
  if (account_chooser_dialog_) {
    account_chooser_dialog_->ControllerGone();
    account_chooser_dialog_ = nullptr;
  }
  if (autosignin_dialog_) {
    autosignin_dialog_->ControllerGone();
    autosignin_dialog_ = nullptr;
  }
}
