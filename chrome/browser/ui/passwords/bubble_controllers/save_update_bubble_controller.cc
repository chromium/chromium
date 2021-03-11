// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/default_clock.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

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
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_store->RemoveSiteStats(origin.GetURL());
}

std::vector<password_manager::PasswordForm> DeepCopyForms(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms) {
  std::vector<password_manager::PasswordForm> result;
  result.reserve(forms.size());
  std::transform(
      forms.begin(), forms.end(), std::back_inserter(result),
      [](const std::unique_ptr<password_manager::PasswordForm>& form) {
        return *form;
      });
  return result;
}

bool IsSyncUser(Profile* profile) {
  const syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return password_bubble_experiment::IsSmartLockUser(sync_service);
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
  // If kEnablePasswordsAccountStorage is enabled, then
  // SaveUpdateWithAccountStoreBubbleController should be used instead of this
  // class.
  DCHECK(!base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  state_ = delegate_->GetState();
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  origin_ = delegate_->GetOrigin();
  pending_password_ = delegate_->GetPendingPassword();
  local_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
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

  if (are_passwords_revealed_when_bubble_is_opened_) {
    delegate_->OnPasswordsRevealed();
  }
  // The condition for the password reauth:
  // If the bubble opened after reauth -> no more reauth necessary, otherwise
  // If a password was autofilled -> require reauth to view it, otherwise
  // Require reauth iff the user opened the bubble manually and it's not the
  // manual saving state. The manual saving state as well as automatic prompt
  // are temporary states, therefore, it's better for the sake of convenience
  // for the user not to break the UX with the reauth prompt.
  password_revealing_requires_reauth_ =
      !are_passwords_revealed_when_bubble_is_opened_ &&
      (pending_password_.form_has_autofilled_value ||
       (!delegate_->BubbleIsManualFallbackForSaving() &&
        display_reason ==
            PasswordBubbleControllerBase::DisplayReason::kUserAction));
  enable_editing_ = delegate_->GetCredentialSource() !=
                    password_manager::metrics_util::CredentialSourceType::
                        kCredentialManagementAPI;
}

SaveUpdateBubbleController::~SaveUpdateBubbleController() {
  if (!interaction_reported_)
    OnBubbleClosing();
}

void SaveUpdateBubbleController::OnSaveClicked() {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  dismissal_reason_ = metrics_util::CLICKED_ACCEPT;
  if (delegate_) {
    CleanStatisticsForSite(GetProfile(), origin_);
    delegate_->SavePassword(pending_password_.username_value,
                            pending_password_.password_value);
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

bool SaveUpdateBubbleController::IsCurrentStateUpdate() const {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_STATE);
  return std::any_of(local_credentials_.begin(), local_credentials_.end(),
                     [this](const password_manager::PasswordForm& form) {
                       return form.username_value ==
                              pending_password_.username_value;
                     });
}

bool SaveUpdateBubbleController::ShouldShowFooter() const {
  return (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
          state_ == password_manager::ui::PENDING_PASSWORD_STATE) &&
         IsSyncUser(GetProfile());
}

bool SaveUpdateBubbleController::ReplaceToShowPromotionIfNeeded() {
  Profile* profile = GetProfile();
  if (!profile)
    return false;
  PrefService* prefs = profile->GetPrefs();
  const syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  // Signin promotion.
  if (password_bubble_experiment::ShouldShowChromeSignInPasswordPromo(
          prefs, sync_service)) {
    ReportInteractions();
    state_ = password_manager::ui::CHROME_SIGN_IN_PROMO_STATE;
    int show_count = prefs->GetInteger(
        password_manager::prefs::kNumberSignInPasswordPromoShown);
    show_count++;
    prefs->SetInteger(password_manager::prefs::kNumberSignInPasswordPromoShown,
                      show_count);
    return true;
  }
  return false;
}

bool SaveUpdateBubbleController::RevealPasswords() {
  bool reveal_immediately = !password_revealing_requires_reauth_ ||
                            (delegate_ && delegate_->AuthenticateUser());
  if (reveal_immediately)
    delegate_->OnPasswordsRevealed();
  return reveal_immediately;
}

std::u16string SaveUpdateBubbleController::GetTitle() const {
  if (state_ == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE)
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SYNC_PROMO_TITLE);

  PasswordTitleType type = IsCurrentStateUpdate()
                               ? PasswordTitleType::UPDATE_PASSWORD
                               : (pending_password_.federation_origin.opaque()
                                      ? PasswordTitleType::SAVE_PASSWORD
                                      : PasswordTitleType::SAVE_ACCOUNT);
  return GetSavePasswordDialogTitleText(GetWebContents()->GetVisibleURL(),
                                        origin_, type);
}

void SaveUpdateBubbleController::ReportInteractions() {
  if (state_ == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE)
    return;
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
        password_manager::PasswordStore* password_store =
            PasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::IMPLICIT_ACCESS)
                .get();
        password_store->AddSiteStats(interaction_stats_);
      }
    }
  }

  // Log UMA histograms.
  if (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    metrics_util::LogUpdateUIDismissalReason(dismissal_reason_);
  } else if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    base::Optional<metrics_util::PasswordAccountStorageUserState> user_state =
        base::nullopt;
    Profile* profile = GetProfile();
    if (profile) {
      user_state = password_manager::features_util::
          ComputePasswordAccountStorageUserState(
              profile->GetPrefs(),
              ProfileSyncServiceFactory::GetForProfile(profile));
    }
    metrics_util::LogSaveUIDismissalReason(dismissal_reason_, user_state);
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
