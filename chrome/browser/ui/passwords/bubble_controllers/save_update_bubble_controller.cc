// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/time/default_clock.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/smart_bubble_stats_store.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using Store = password_manager::PasswordForm::Store;
namespace metrics_util = password_manager::metrics_util;
namespace features_util = password_manager::features_util;

metrics_util::UIDisplayDisposition ComputeDisplayDisposition(
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
    }
  } else {
    switch (state) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        return metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        return metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE;
      default:
        NOTREACHED();
    }
  }
}

void CleanStatisticsForSite(Profile* profile, const url::Origin& origin) {
  CHECK(profile);
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::SmartBubbleStatsStore* stats_store =
      password_store->GetSmartBubbleStatsStore();
  if (stats_store) {
    stats_store->RemoveSiteStats(origin.GetURL());
  }
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
  return password_manager::sync_util::IsSyncFeatureActiveIncludingPasswords(
      sync_service);
}

}  // namespace

SaveUpdateBubbleController::SaveUpdateBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    PasswordBubbleControllerBase::DisplayReason display_reason)
    : CommonSavedAccountManagerBubbleController(
          delegate,
          display_reason,
          ComputeDisplayDisposition(display_reason, delegate->GetState())),
      clock_(base::DefaultClock::GetInstance()) {
  existing_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
  original_username_ = GetPendingPassword().username_value;
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
      (GetPendingPassword().form_has_autofilled_value ||
       !delegate_->BubbleIsManualFallbackForSaving());
  if (GetState() == password_manager::ui::PENDING_PASSWORD_STATE) {
    interaction_stats_.origin_domain = GetOrigin().GetURL();
    interaction_stats_.username_value = GetPendingPassword().username_value;
    const password_manager::InteractionsStats* stats =
        delegate_->GetCurrentInteractionStats();
    if (stats) {
      CHECK_EQ(interaction_stats_.username_value, stats->username_value);
      CHECK_EQ(interaction_stats_.origin_domain, stats->origin_domain);
      interaction_stats_.dismissal_count = stats->dismissal_count;
    }
  }
}

SaveUpdateBubbleController::~SaveUpdateBubbleController() {
  OnBubbleClosing();
}

void SaveUpdateBubbleController::OnSaveClicked() {
  CHECK(GetState() == password_manager::ui::PENDING_PASSWORD_STATE ||
        GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  SetDismissalReason(metrics_util::CLICKED_ACCEPT);
  if (delegate_) {
    CleanStatisticsForSite(GetProfile(), GetOrigin());
    if (IsAccountStorageOptInRequiredBeforeSave()) {
      delegate_->AuthenticateUserForAccountStoreOptInAndSavePassword(
          GetPendingPassword().username_value,
          GetPendingPassword().password_value);
    } else {
      delegate_->SavePassword(GetPendingPassword().username_value,
                              GetPendingPassword().password_value);
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

void SaveUpdateBubbleController::OnNeverForThisSiteClicked() {
  CHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  SetDismissalReason(metrics_util::CLICKED_NEVER);
  if (delegate_) {
    CleanStatisticsForSite(GetProfile(), GetOrigin());
    delegate_->NeverSavePassword();
  }
}

bool SaveUpdateBubbleController::IsCurrentStateUpdate() const {
  CHECK(GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
        GetState() == password_manager::ui::PENDING_PASSWORD_STATE);
  // If the username didn't change use the `delegate_->GetState()` to determine
  // whether the operation will save a new credential or update existing. This
  // is because in some cases PSL matches might be updated depending on a
  // context. For example, if PSL password is filled on change password flow
  // together with a new password, we should update existing credential. On the
  // other hand if PSL matched credential is filled on sign-in page and password
  // is updated by user we want to save a new password instead.
  if (original_username_ == GetPendingPassword().username_value) {
    return GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
  }
  return base::Contains(existing_credentials_,
                        GetPendingPassword().username_value,
                        &password_manager::PasswordForm::username_value);
}

bool SaveUpdateBubbleController::ShouldShowFooter() const {
  return (GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
          GetState() == password_manager::ui::PENDING_PASSWORD_STATE) &&
         IsSyncUser(GetProfile());
}

bool SaveUpdateBubbleController::
    IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount() {
  CHECK(GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
        GetState() == password_manager::ui::PENDING_PASSWORD_STATE);

  if (!delegate_) {
    // A race between `PasswordBubbleControllerBase::OnBubbleClosing()` and
    // `PasswordSaveUpdateView::OnContentChanged()` may result in a null
    // `delegate_`.
    return false;
  }
  if (IsSyncUser(GetProfile())) {
    return true;
  }

  bool is_update = false;
  bool is_update_in_account_store = false;
  for (const password_manager::PasswordForm& form : existing_credentials_) {
    if (form.username_value == GetPendingPassword().username_value) {
      is_update = true;
      if (form.IsUsingAccountStore()) {
        is_update_in_account_store = true;
      }
    }
  }

  if (!is_update) {
    return IsUsingAccountStore();
  }

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
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
#elif BUILDFLAG(IS_WIN)
  message = l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
#endif
  // Bind OnUserAuthenticationCompleted() using a weak_ptr such that if the
  // bubble is closed (and controller is destructed) while the reauth flow is
  // running, no callback will be invoked upon the conclusion of the
  // authentication flow.
  delegate_->AuthenticateUserWithMessage(
      message,
      base::BindOnce(&CommonSavedAccountManagerBubbleController::
                         OnUserAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
  if (IsCurrentStateUpdate()) {
    return false;
  }
  // If saving to the profile store, then no need to ask for opt-in.
  if (!IsUsingAccountStore()) {
    return false;
  }
  // If already opted in, no need to ask again.
  if (delegate_->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    return false;
  }

  return true;
}

bool SaveUpdateBubbleController::DidAuthForAccountStoreOptInFail() const {
  return delegate_->DidAuthForAccountStoreOptInFail();
}

std::u16string SaveUpdateBubbleController::GetTitle() const {
  PasswordTitleType type = IsCurrentStateUpdate()
                               ? PasswordTitleType::UPDATE_PASSWORD
                               : (GetPendingPassword().IsFederatedCredential()
                                      ? PasswordTitleType::SAVE_ACCOUNT
                                      : PasswordTitleType::SAVE_PASSWORD);
  return GetSavePasswordDialogTitleText(GetWebContents()->GetVisibleURL(),
                                        GetOrigin(), type);
}

void SaveUpdateBubbleController::ReportInteractions() {
  CHECK(GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
        GetState() == password_manager::ui::PENDING_PASSWORD_STATE);
  if (GetState() == password_manager::ui::PENDING_PASSWORD_STATE) {
    // Update the statistics for the save password bubble.
    Profile* profile = GetProfile();
    if (profile) {
      if (GetDismissalReason() == metrics_util::NO_DIRECT_INTERACTION &&
          GetDisplayDisposition() ==
              metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING) {
        if (interaction_stats_.dismissal_count <
            std::numeric_limits<
                decltype(interaction_stats_.dismissal_count)>::max()) {
          interaction_stats_.dismissal_count++;
        }
        interaction_stats_.update_time = clock_->Now();
        password_manager::PasswordStoreInterface* password_store =
            ProfilePasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::IMPLICIT_ACCESS)
                .get();
        password_manager::SmartBubbleStatsStore* stats_store =
            password_store->GetSmartBubbleStatsStore();
        if (stats_store) {
          stats_store->AddSiteStats(interaction_stats_);
        }
      }
    }
  }

  // Log UMA histograms.
  if (GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    metrics_util::LogUpdateUIDismissalReason(GetDismissalReason());
  } else if (GetState() == password_manager::ui::PENDING_PASSWORD_STATE) {
    std::optional<features_util::PasswordAccountStorageUserState> user_state =
        std::nullopt;
    Profile* profile = GetProfile();
    if (profile) {
      user_state = password_manager::features_util::
          ComputePasswordAccountStorageUserState(
              profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
    }

    // Log additional UMA for users who don't yet have any passwords saved in
    // the password manager (in both profile and account stores) to measure
    // saving adoption.
    const bool log_adoption_metric =
        profile &&
        !profile->GetPrefs()->GetBoolean(
            password_manager::prefs::
                kAutofillableCredentialsProfileStoreLoginDatabase) &&
        !profile->GetPrefs()->GetBoolean(
            password_manager::prefs::
                kAutofillableCredentialsAccountStoreLoginDatabase);
    metrics_util::LogSaveUIDismissalReason(GetDismissalReason(), user_state,
                                           log_adoption_metric);
  }

  // Update the delegate so that it can send votes to the server.
  // Send a notification if there was no interaction with the bubble.
  bool no_interaction =
      GetDismissalReason() == metrics_util::NO_DIRECT_INTERACTION;
  if (no_interaction && delegate_) {
    delegate_->OnNoInteraction();
  }

  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_) {
    metrics_recorder_->RecordUIDismissalReason(GetDismissalReason());
  }
}
