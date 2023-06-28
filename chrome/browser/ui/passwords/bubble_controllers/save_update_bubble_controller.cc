// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "components/password_manager/core/browser/smart_bubble_stats_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

namespace {

namespace metrics_util = password_manager::metrics_util;
using Store = password_manager::PasswordForm::Store;

password_manager::metrics_util::UIDisplayDisposition ComputeDisplayDisposition(
    PasswordBubbleControllerBase::DisplayReason display_reason,
    password_manager::ui::State state) {
  if (display_reason ==
      PasswordBubbleControllerBase::DisplayReason::kUserAction) {
    switch (state) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        return metrics_util::MANUAL_WITH_PASSWORD_PENDING;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        return metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE;
      default:
        NOTREACHED();
        return metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE;
    }
  } else {
    switch (state) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        return metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        return metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE;
      default:
        NOTREACHED();
        return metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
    }
  }
}

void CleanStatisticsForSite(Profile* profile, const url::Origin& origin) {
  DCHECK(profile);
  password_manager::PasswordStoreInterface* password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::SmartBubbleStatsStore* stats_store =
      password_store->GetSmartBubbleStatsStore();
  if (stats_store)
    stats_store->RemoveSiteStats(origin.GetURL());
}

std::vector<password_manager::PasswordForm> DeepCopyForms(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms) {
  std::vector<password_manager::PasswordForm> result;
  result.reserve(forms.size());
  base::ranges::transform(
      forms, std::back_inserter(result),
      &std::unique_ptr<password_manager::PasswordForm>::operator*);
  return result;
}

bool IsSyncUser(Profile* profile) {
  CHECK(profile);
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  password_manager::SyncState sync_state =
      password_manager_util::GetPasswordSyncState(sync_service);
  return sync_state == password_manager::SyncState::kSyncingNormalEncryption ||
         sync_state ==
             password_manager::SyncState::kSyncingWithCustomPassphrase;
}

}  // namespace

SaveUpdateBubbleController::SaveUpdateBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    PasswordBubbleControllerBase::DisplayReason display_reason)
    : PasswordBubbleControllerBase(
          delegate,
          ComputeDisplayDisposition(display_reason, delegate->GetState())),
      display_disposition_(
          ComputeDisplayDisposition(display_reason, delegate->GetState())),
      password_revealing_requires_reauth_(false),
      enable_editing_(false),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION),
      clock_(base::DefaultClock::GetInstance()) {
  state_ = delegate_->GetState();
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  origin_ = delegate_->GetOrigin();
  pending_password_ = delegate_->GetPendingPassword();
  existing_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    interaction_stats_.origin_domain = origin_.GetURL();
    interaction_stats_.username_value = pending_password_.username_value;
    const password_manager::InteractionsStats* stats =
        delegate_->GetCurrentInteractionStats();
    if (stats) {
      DCHECK_EQ(interaction_stats_.username_value, stats->username_value);
      DCHECK_EQ(interaction_stats_.origin_domain, stats->origin_domain);
      interaction_stats_.dismissal_count = stats->dismissal_count;
    }
  }
  // The condition for the password reauth:
  // If the bubble opened after successful submission -> no reauth because it's
  // a temporary state and we should not complicate that UX flow.
  // If a password was autofilled -> require reauth to view it.
  // If the bubble opened manually and not a manual fallback -> require reauth.
  // The manual fallback is a temporary state and it's better for the sake of
  // convenience for the user not to break the UX with the reauth prompt.
  password_revealing_requires_reauth_ =
      display_reason ==
          PasswordBubbleControllerBase::DisplayReason::kUserAction &&
      (pending_password_.form_has_autofilled_value ||
       !delegate_->BubbleIsManualFallbackForSaving());
  enable_editing_ = delegate_->GetCredentialSource() !=
                    password_manager::metrics_util::CredentialSourceType::
                        kCredentialManagementAPI;
}

SaveUpdateBubbleController::~SaveUpdateBubbleController() {
  OnBubbleClosing();
}

void SaveUpdateBubbleController::OnSaveClicked() {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  dismissal_reason_ = metrics_util::CLICKED_ACCEPT;
  if (delegate_) {
    CleanStatisticsForSite(GetProfile(), origin_);
    if (IsAccountStorageOptInRequiredBeforeSave()) {
      delegate_->AuthenticateUserForAccountStoreOptInAndSavePassword(
          pending_password_.username_value, pending_password_.password_value);
    } else {
      delegate_->SavePassword(pending_password_.username_value,
                              pending_password_.password_value);
      if (!IsCurrentStateUpdate() &&
          delegate_->GetPasswordFeatureManager()
              ->ShouldOfferOptInAndMoveToAccountStoreAfterSavingLocally()) {
        delegate_
            ->AuthenticateUserForAccountStoreOptInAfterSavingLocallyAndMovePassword();
      } else {
        delegate_->MaybeShowIOSPasswordPromo();
      }
    }
  }
}

void SaveUpdateBubbleController::OnNopeUpdateClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE, state_);
  dismissal_reason_ = metrics_util::CLICKED_CANCEL;
  if (delegate_)
    delegate_->OnNopeUpdateClicked();
}

void SaveUpdateBubbleController::OnNeverForThisSiteClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  dismissal_reason_ = metrics_util::CLICKED_NEVER;
  if (delegate_) {
    CleanStatisticsForSite(GetProfile(), origin_);
    delegate_->NeverSavePassword();
  }
}

void SaveUpdateBubbleController::OnCredentialEdited(
    std::u16string new_username,
    std::u16string new_password) {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  pending_password_.username_value = std::move(new_username);
  pending_password_.password_value = std::move(new_password);
}

void SaveUpdateBubbleController::OnGooglePasswordManagerLinkClicked() {
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::kSaveUpdateBubble);
  }
}

bool SaveUpdateBubbleController::IsCurrentStateUpdate() const {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_STATE);
  return base::Contains(existing_credentials_, pending_password_.username_value,
                        &password_manager::PasswordForm::username_value);
}

bool SaveUpdateBubbleController::ShouldShowFooter() const {
  return (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
          state_ == password_manager::ui::PENDING_PASSWORD_STATE) &&
         IsSyncUser(GetProfile());
}

bool SaveUpdateBubbleController::
    IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount() {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_STATE);

  if (!delegate_) {
    // A race between `PasswordBubbleControllerBase::OnBubbleClosing()` and
    // `PasswordSaveUpdateView::OnContentChanged()` may result in a null
    // `delegate_`.
    return false;
  }
  if (IsSyncUser(GetProfile()))
    return true;

  bool is_update = false;
  bool is_update_in_account_store = false;
  for (const password_manager::PasswordForm& form : existing_credentials_) {
    if (form.username_value == pending_password_.username_value) {
      is_update = true;
      if (form.IsUsingAccountStore())
        is_update_in_account_store = true;
    }
  }

  if (!is_update)
    return IsUsingAccountStore();

  return is_update_in_account_store;
}

void SaveUpdateBubbleController::ShouldRevealPasswords(
    PasswordsModelDelegate::AvailabilityCallback callback) {
  // Password can be revealed immediately.
  if (!delegate_ || !password_revealing_requires_reauth_) {
    delegate_->OnPasswordsRevealed();
    std::move(callback).Run(true);
    return;
  }

  std::u16string message;
#if BUILDFLAG(IS_MAC)
  message = password_manager_util_mac::GetMessageForLoginPrompt(
      password_manager::ReauthPurpose::VIEW_PASSWORD);
#elif BUILDFLAG(IS_WIN)
  message = password_manager_util_win::GetMessageForLoginPrompt(
      password_manager::ReauthPurpose::VIEW_PASSWORD);
#endif
  // Bind OnUserAuthenticationCompleted() using a weak_ptr such that if the
  // bubble is closed (and controller is destructed) while the reauth flow is
  // running, no callback will be invoked upon the conclusion of the
  // authentication flow.
  delegate_->AuthenticateUserWithMessage(
      message,
      base::BindOnce(&SaveUpdateBubbleController::OnUserAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool SaveUpdateBubbleController::ShouldShowPasswordStorePicker() const {
  if (!delegate_->GetPasswordFeatureManager()
           ->ShouldShowAccountStorageBubbleUi()) {
    return false;
  }
  if (delegate_->GetPasswordFeatureManager()
          ->ShouldOfferOptInAndMoveToAccountStoreAfterSavingLocally()) {
    // If the user will be asked to opt-in *after* saving the current password
    // locally, then do not show the destination picker yet.
    DCHECK_EQ(delegate_->GetPasswordFeatureManager()->GetDefaultPasswordStore(),
              Store::kProfileStore);
    return false;
  }
  return true;
}

void SaveUpdateBubbleController::OnToggleAccountStore(
    bool is_account_store_selected) {
  delegate_->GetPasswordFeatureManager()->SetDefaultPasswordStore(
      is_account_store_selected ? Store::kAccountStore : Store::kProfileStore);
}

bool SaveUpdateBubbleController::IsUsingAccountStore() {
  return delegate_->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
         Store::kAccountStore;
}

bool SaveUpdateBubbleController::IsAccountStorageOptInRequiredBeforeSave() {
  // If this is an update, either a) the password only exists in the profile
  // store, so the opt-in shouldn't be offered because the account storage won't
  // be used, or b) there is a copy in the account store, which means the user
  // already opted in. Either way, the opt-in shouldn't be offered.
  if (IsCurrentStateUpdate())
    return false;
  // If saving to the profile store, then no need to ask for opt-in.
  if (!IsUsingAccountStore())
    return false;
  // If already opted in, no need to ask again.
  if (delegate_->GetPasswordFeatureManager()->IsOptedInForAccountStorage())
    return false;

  return true;
}

std::u16string SaveUpdateBubbleController::GetPrimaryAccountEmail() {
  Profile* profile = GetProfile();
  if (!profile)
    return std::u16string();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return std::u16string();
  return base::UTF8ToUTF16(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

ui::ImageModel SaveUpdateBubbleController::GetPrimaryAccountAvatar(
    int icon_size_dip) {
  Profile* profile = GetProfile();
  if (!profile)
    return ui::ImageModel();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return ui::ImageModel();
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  DCHECK(!primary_account_info.IsEmpty());
  gfx::Image account_icon = primary_account_info.account_image;
  if (account_icon.IsEmpty()) {
    account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
      account_icon, icon_size_dip, icon_size_dip, profiles::SHAPE_CIRCLE));
}

bool SaveUpdateBubbleController::DidAuthForAccountStoreOptInFail() const {
  return delegate_->DidAuthForAccountStoreOptInFail();
}

std::u16string SaveUpdateBubbleController::GetTitle() const {
  PasswordTitleType type = IsCurrentStateUpdate()
                               ? PasswordTitleType::UPDATE_PASSWORD
                               : (pending_password_.federation_origin.opaque()
                                      ? PasswordTitleType::SAVE_PASSWORD
                                      : PasswordTitleType::SAVE_ACCOUNT);
  return GetSavePasswordDialogTitleText(GetWebContents()->GetVisibleURL(),
                                        origin_, type);
}

void SaveUpdateBubbleController::ReportInteractions() {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_STATE);
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    // Update the statistics for the save password bubble.
    Profile* profile = GetProfile();
    if (profile) {
      if (dismissal_reason_ == metrics_util::NO_DIRECT_INTERACTION &&
          display_disposition_ ==
              metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING) {
        if (interaction_stats_.dismissal_count <
            std::numeric_limits<decltype(
                interaction_stats_.dismissal_count)>::max())
          interaction_stats_.dismissal_count++;
        interaction_stats_.update_time = clock_->Now();
        password_manager::PasswordStoreInterface* password_store =
            PasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::IMPLICIT_ACCESS)
                .get();
        password_manager::SmartBubbleStatsStore* stats_store =
            password_store->GetSmartBubbleStatsStore();
        if (stats_store)
          stats_store->AddSiteStats(interaction_stats_);
      }
    }
  }

  // Log UMA histograms.
  if (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    metrics_util::LogUpdateUIDismissalReason(
        dismissal_reason_, pending_password_.submission_event);
  } else if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    absl::optional<metrics_util::PasswordAccountStorageUserState> user_state =
        absl::nullopt;
    Profile* profile = GetProfile();
    if (profile) {
      user_state = password_manager::features_util::
          ComputePasswordAccountStorageUserState(
              profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
    }
    metrics_util::LogSaveUIDismissalReason(
        dismissal_reason_, pending_password_.submission_event, user_state);
  }

  // Update the delegate so that it can send votes to the server.
  // Send a notification if there was no interaction with the bubble.
  bool no_interaction =
      dismissal_reason_ == metrics_util::NO_DIRECT_INTERACTION;
  if (no_interaction && delegate_) {
    delegate_->OnNoInteraction();
  }

  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_)
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
}

void SaveUpdateBubbleController::OnUserAuthenticationCompleted(
    base::OnceCallback<void(bool)> completion,
    bool authentication_result) {
  if (authentication_result) {
    delegate_->OnPasswordsRevealed();
  }
  std::move(completion).Run(authentication_result);
}
