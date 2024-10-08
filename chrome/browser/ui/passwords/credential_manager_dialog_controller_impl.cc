// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_manager_dialog_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
std::u16string GetAuthenticationMessage(PasswordsModelDelegate* delegate) {
  std::u16string message;
  if (!delegate || !delegate->GetWebContents()) {
    return u"";
  }
  const std::u16string origin = base::UTF8ToUTF16(
      password_manager::GetShownOrigin(delegate->GetWebContents()
                                           ->GetPrimaryMainFrame()
                                           ->GetLastCommittedOrigin()));
  message =
      l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
  return message;
}
#endif

}  // namespace

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
    std::vector<std::unique_ptr<password_manager::PasswordForm>> locals) {
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

std::u16string CredentialManagerDialogControllerImpl::GetAccountChooserTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_TITLE);
}

bool CredentialManagerDialogControllerImpl::ShouldShowSignInButton() const {
  return local_credentials_.size() == 1;
}

std::u16string CredentialManagerDialogControllerImpl::GetAutoSigninPromoTitle()
    const {
  int message_id = IsSyncingAutosignSetting(profile_)
                       ? IDS_AUTO_SIGNIN_FIRST_RUN_TITLE_MANY_DEVICES
                       : IDS_AUTO_SIGNIN_FIRST_RUN_TITLE_LOCAL_DEVICE;
  return l10n_util::GetStringUTF16(message_id);
}

std::u16string CredentialManagerDialogControllerImpl::GetAutoSigninText()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTO_SIGNIN_FIRST_RUN_TEXT,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TITLE_BRAND));
}

bool CredentialManagerDialogControllerImpl::ShouldShowFooter() const {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  // TODO(crbug.com/40067296): Migrate away from `ConsentLevel::kSync` on
  // desktop platforms and remove #ifdef below.
#if BUILDFLAG(IS_ANDROID)
#error If this code is built on Android, please update TODO above.
#endif  // BUILDFLAG(IS_ANDROID)
  return password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
      sync_service);
}

void CredentialManagerDialogControllerImpl::OnChooseCredentials(
    const password_manager::PasswordForm& password_form,
    password_manager::CredentialType credential_type) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  if (delegate_->GetPasswordFeatureManager()
          ->IsBiometricAuthenticationBeforeFillingEnabled()) {
    delegate_->AuthenticateUserWithMessage(
        GetAuthenticationMessage(delegate_),
        base::BindOnce(
            &CredentialManagerDialogControllerImpl::OnBiometricReauthCompleted,
            weak_ptr_factory_.GetWeakPtr(), password_form, credential_type));
    return;
  }
#endif
  ResetDialog();
  delegate_->ChooseCredential(password_form, credential_type);
}

void CredentialManagerDialogControllerImpl::OnSignInClicked() {
  CHECK_EQ(1u, local_credentials_.size());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  if (delegate_->GetPasswordFeatureManager()
          ->IsBiometricAuthenticationBeforeFillingEnabled()) {
    delegate_->AuthenticateUserWithMessage(
        GetAuthenticationMessage(delegate_),
        base::BindOnce(
            &CredentialManagerDialogControllerImpl::OnBiometricReauthCompleted,
            weak_ptr_factory_.GetWeakPtr(), *local_credentials_[0],
            password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
    return;
  }
#endif
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

void CredentialManagerDialogControllerImpl::OnBiometricReauthCompleted(
    password_manager::PasswordForm password_form,
    password_manager::CredentialType credential_type,
    bool result) {
  if (!result) {
    return;
  }
  ResetDialog();
  delegate_->ChooseCredential(password_form, credential_type);
}
