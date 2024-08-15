// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/manage_passwords_bubble_controller.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_util.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;
namespace {

using password_manager::metrics_util::PasswordManagementBubbleInteractions;

// Reports a metric based on the change between the `current_note` and the
// `updated_note`.
void LogNoteChangesInPasswordManagementBubble(
    const std::optional<std::u16string>& current_note,
    const std::optional<std::u16string>& updated_note) {
  std::u16string current_note_value = current_note.value_or(std::u16string());
  std::u16string updated_note_value = updated_note.value_or(std::u16string());
  if (current_note_value == updated_note_value) {
    return;
  }

  if (current_note_value.empty()) {
    metrics_util::LogUserInteractionsInPasswordManagementBubble(
        metrics_util::PasswordManagementBubbleInteractions::kNoteAdded);
    return;
  }
  if (updated_note_value.empty()) {
    metrics_util::LogUserInteractionsInPasswordManagementBubble(
        metrics_util::PasswordManagementBubbleInteractions::kNoteDeleted);
    return;
  }

  metrics_util::LogUserInteractionsInPasswordManagementBubble(
      metrics_util::PasswordManagementBubbleInteractions::kNoteEdited);
}

}  // namespace

ManagePasswordsBubbleController::ManagePasswordsBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          /*display_disposition=*/metrics_util::MANUAL_MANAGE_PASSWORDS),
      bubble_mode_(
          delegate_ &&
                  delegate_
                      ->GetManagePasswordsSingleCredentialDetailsModeCredential()
              ? BubbleMode::kSingleCredentialDetails
              : BubbleMode::kCredentialList),
      details_bubble_credential_(
          delegate_
              ? delegate_
                    ->GetManagePasswordsSingleCredentialDetailsModeCredential()
              : std::nullopt) {}

ManagePasswordsBubbleController::~ManagePasswordsBubbleController() {
  OnBubbleClosing();
}

std::u16string ManagePasswordsBubbleController::GetTitle() const {
  if (!delegate_) {
    return std::u16string();
  }

  switch (delegate_->GetState()) {
    case password_manager::ui::SAVE_CONFIRMATION_STATE:
      return GetConfirmationManagePasswordsDialogTitleText(/*is_update=*/false);
    case password_manager::ui::UPDATE_CONFIRMATION_STATE:
      return GetConfirmationManagePasswordsDialogTitleText(/*is_update=*/true);
    case password_manager::ui::MANAGE_STATE: {
      switch (bubble_mode_) {
        case BubbleMode::kCredentialList:
          return GetManagePasswordsDialogTitleText(
              GetWebContents()->GetVisibleURL(), delegate_->GetOrigin(),
              !delegate_->GetCurrentForms().empty());
        case BubbleMode::kSingleCredentialDetails:
          const std::vector<password_manager::CredentialUIEntry::DomainInfo>&
              affiliated_domains = password_manager::CredentialUIEntry(
                                       *details_bubble_credential_)
                                       .GetAffiliatedDomains();
          CHECK(!affiliated_domains.empty());
          return base::UTF8ToUTF16(affiliated_domains[0].name);
      }
    }
    default:
      NOTREACHED();
  }
}

void ManagePasswordsBubbleController::OnManageClicked(
    password_manager::ManagePasswordsReferrer referrer) {
  dismissal_reason_ = metrics_util::CLICKED_MANAGE;
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(referrer);
  }
}

void ManagePasswordsBubbleController::OnManagePasswordClicked(
    password_manager::ManagePasswordsReferrer referrer) {
  CHECK(details_bubble_credential_.has_value());
  dismissal_reason_ = metrics_util::CLICKED_MANAGE_PASSWORD;
  if (delegate_) {
    std::vector<password_manager::CredentialUIEntry::DomainInfo>
        affiliated_domains = password_manager::CredentialUIEntry(
                                 details_bubble_credential_.value())
                                 .GetAffiliatedDomains();
    CHECK(!affiliated_domains.empty());
    // Any `affiliated_domains[i].name` can used to navigate to the credential
    // details page.
    delegate_->NavigateToPasswordDetailsPageInPasswordManager(
        affiliated_domains[0].name, referrer);
  }
}

void ManagePasswordsBubbleController::RequestFavicon(
    base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon::GetFaviconImageForPageURL(
      favicon_service, GetWebContents()->GetVisibleURL(),
      favicon_base::IconType::kFavicon,
      base::BindOnce(&ManagePasswordsBubbleController::OnFaviconReady,
                     base::Unretained(this), std::move(favicon_ready_callback)),
      &favicon_tracker_);
}

ManagePasswordsBubbleController::SyncState
ManagePasswordsBubbleController::GetPasswordSyncState() const {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  switch (password_manager::sync_util::GetPasswordSyncState(sync_service)) {
    case password_manager::sync_util::SyncState::kNotActive:
      return SyncState::kNotActive;
    case password_manager::sync_util::SyncState::kActiveWithNormalEncryption:
    case password_manager::sync_util::SyncState::kActiveWithCustomPassphrase:
      return sync_service->IsSyncFeatureEnabled()
                 ? SyncState::kActiveWithSyncFeatureEnabled
                 : SyncState::kActiveWithAccountPasswords;
  }
  NOTREACHED();
}

std::u16string ManagePasswordsBubbleController::GetPrimaryAccountEmail() {
  Profile* profile = GetProfile();
  if (!profile) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

void ManagePasswordsBubbleController::OnGooglePasswordManagerLinkClicked() {
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::kManagePasswordsBubble);
  }
}

void ManagePasswordsBubbleController::OnMovePasswordLinkClicked() {
  CHECK(details_bubble_credential_.has_value());
  password_manager::metrics_util::LogUserInteractionsInPasswordManagementBubble(
      PasswordManagementBubbleInteractions::kMovePasswordLinkClicked);
  if (delegate_) {
    delegate_->ShowMovePasswordBubble(details_bubble_credential_.value());
  }
}

base::span<std::unique_ptr<password_manager::PasswordForm> const>
ManagePasswordsBubbleController::GetCredentials() const {
  if (!delegate_) {
    return base::span<std::unique_ptr<password_manager::PasswordForm> const>();
  }
  return base::make_span(delegate_->GetCurrentForms());
}

const password_manager::PasswordForm&
ManagePasswordsBubbleController::GetSingleCredentialDetailsModeCredential()
    const {
  CHECK_EQ(bubble_mode_, BubbleMode::kSingleCredentialDetails);
  return *delegate_->GetManagePasswordsSingleCredentialDetailsModeCredential();
}

void ManagePasswordsBubbleController::
    UpdateDetailsBubbleCredentialInPasswordStore(
        password_manager::PasswordForm updated_form) {
  DCHECK(details_bubble_credential_.has_value());
  Profile* profile = GetProfile();
  if (!profile) {
    return;
  }
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      PasswordStoreForForm(details_bubble_credential_.value());

  LogNoteChangesInPasswordManagementBubble(
      details_bubble_credential_->GetNoteWithEmptyUniqueDisplayName(),
      updated_form.GetNoteWithEmptyUniqueDisplayName());

  if (details_bubble_credential_.value().username_value ==
      updated_form.username_value) {
    password_store->UpdateLogin(updated_form);
    details_bubble_credential_ = updated_form;
    return;
  }
  if (updated_form.username_value.empty()) {
    // The UI doesn't allow clearing the username.
    NOTREACHED_IN_MIGRATION();
    return;
  }
  // The UI allows updating the username for credentials with an empty username.
  // Since the username is part of the the unique key, updating it requires
  // calling another API on the password store.

  // Phished and leaked issues are no longer relevant on username change.
  // Weak and reused issues are still relevant.
  updated_form.password_issues.erase(password_manager::InsecureType::kPhished);
  updated_form.password_issues.erase(password_manager::InsecureType::kLeaked);
  password_store->UpdateLoginWithPrimaryKey(updated_form,
                                            details_bubble_credential_.value());
  details_bubble_credential_ = updated_form;

  metrics_util::LogUserInteractionsInPasswordManagementBubble(
      metrics_util::PasswordManagementBubbleInteractions::kUsernameAdded);
}

void ManagePasswordsBubbleController::AuthenticateUserAndDisplayDetailsOf(
    password_manager::PasswordForm password_form,
    base::OnceCallback<void(bool)> completion) {
  if (!delegate_) {
    std::move(completion).Run(false);
    return;
  }

  std::u16string message;
#if BUILDFLAG(IS_MAC)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
#elif BUILDFLAG(IS_WIN)
  message = l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
#endif
  // Bind OnUserAuthenticationCompleted() using a weak_ptr such that if the
  // bubble is closed (and controller is destructed) while the reauth flow is
  // running, no callback will be invoked upon the conclusion of the
  // authentication flow.
  auto on_reath_complete = base::BindOnce(
      &ManagePasswordsBubbleController::OnUserAuthenticationCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(password_form),
      std::move(completion));
  delegate_->AuthenticateUserWithMessage(
      message, metrics_util::TimeCallbackMediumTimes(
                   std::move(on_reath_complete),
                   "PasswordManager.ManagementBubble.AuthenticationTime2"));
}

bool ManagePasswordsBubbleController::UsernameExists(
    const std::u16string& username) {
  if (!delegate_) {
    return false;
  }
  return base::ranges::any_of(
      GetCredentials(),
      [&username](const std::unique_ptr<password_manager::PasswordForm>& form) {
        return form->username_value == username;
      });
}

bool ManagePasswordsBubbleController::IsOptedInForAccountStorage() const {
  if (!delegate_) {
    return false;
  }
  return delegate_->GetPasswordFeatureManager()->IsOptedInForAccountStorage();
}

void ManagePasswordsBubbleController::OnFaviconReady(
    base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback,
    const favicon_base::FaviconImageResult& result) {
  std::move(favicon_ready_callback).Run(result.image);
}

void ManagePasswordsBubbleController::ReportInteractions() {
  metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_) {
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
  }
}

scoped_refptr<password_manager::PasswordStoreInterface>
ManagePasswordsBubbleController::PasswordStoreForForm(
    const password_manager::PasswordForm& password_form) const {
  Profile* profile = GetProfile();
  DCHECK(profile);
  return password_form.IsUsingAccountStore()
             ? AccountPasswordStoreFactory::GetForProfile(
                   profile, ServiceAccessType::EXPLICIT_ACCESS)
             : ProfilePasswordStoreFactory::GetForProfile(
                   profile, ServiceAccessType::EXPLICIT_ACCESS);
}

void ManagePasswordsBubbleController::OnUserAuthenticationCompleted(
    password_manager::PasswordForm password_form,
    base::OnceCallback<void(bool)> completion,
    bool authentication_result) {
  if (authentication_result) {
    details_bubble_credential_ = std::move(password_form);
  }
  std::move(completion).Run(authentication_result);
}
