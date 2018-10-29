// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"
#endif

namespace metrics_util = password_manager::metrics_util;

namespace {

void CleanStatisticsForSite(Profile* profile, const GURL& origin) {
  DCHECK(profile);
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_store->RemoveSiteStats(origin.GetOrigin());
}

std::vector<autofill::PasswordForm> DeepCopyForms(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms) {
  std::vector<autofill::PasswordForm> result;
  result.reserve(forms.size());
  std::transform(forms.begin(), forms.end(), std::back_inserter(result),
                 [](const std::unique_ptr<autofill::PasswordForm>& form) {
                   return *form;
                 });
  return result;
}

bool IsSyncUser(Profile* profile) {
  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return password_bubble_experiment::IsSmartLockUser(sync_service);
}

}  // namespace

// Class responsible for collecting and reporting all the runtime interactions
// with the bubble.
class ManagePasswordsBubbleModel::InteractionKeeper {
 public:
  InteractionKeeper(
      password_manager::InteractionsStats stats,
      password_manager::metrics_util::UIDisplayDisposition display_disposition);

  ~InteractionKeeper() = default;

  // Records UMA events, updates the interaction statistics and sends
  // notifications to the delegate when the bubble is closed.
  void ReportInteractions(const ManagePasswordsBubbleModel* model);

  void set_dismissal_reason(
      password_manager::metrics_util::UIDismissalReason reason) {
    dismissal_reason_ = reason;
  }

  void set_sign_in_promo_dismissal_reason(
      password_manager::metrics_util::SyncSignInUserAction reason) {
    sign_in_promo_dismissal_reason_ = reason;
  }

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

  void set_sign_in_promo_shown_count(int count) {
    sign_in_promo_shown_count = count;
  }

 private:
  // The way the bubble appeared.
  const password_manager::metrics_util::UIDisplayDisposition
  display_disposition_;

  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;

  // Dismissal reason for the Chrome Sign in bubble.
  password_manager::metrics_util::SyncSignInUserAction
      sign_in_promo_dismissal_reason_;

  // Current statistics for the save password bubble;
  password_manager::InteractionsStats interaction_stats_;

  // Used to retrieve the current time, in base::Time units.
  base::Clock* clock_;

  // Number of times the sign-in promo was shown to the user.
  int sign_in_promo_shown_count;

  DISALLOW_COPY_AND_ASSIGN(InteractionKeeper);
};

ManagePasswordsBubbleModel::InteractionKeeper::InteractionKeeper(
    password_manager::InteractionsStats stats,
    password_manager::metrics_util::UIDisplayDisposition display_disposition)
    : display_disposition_(display_disposition),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION),
      sign_in_promo_dismissal_reason_(metrics_util::CHROME_SIGNIN_DISMISSED),
      interaction_stats_(std::move(stats)),
      clock_(base::DefaultClock::GetInstance()),
      sign_in_promo_shown_count(0) {}

void ManagePasswordsBubbleModel::InteractionKeeper::ReportInteractions(
    const ManagePasswordsBubbleModel* model) {
  if (model->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    // Update the statistics for the save password bubble.
    Profile* profile = model->GetProfile();
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
                profile, ServiceAccessType::IMPLICIT_ACCESS).get();
        password_store->AddSiteStats(interaction_stats_);
      }
    }
  }

  // Log UMA histograms.
  if (model->state() == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE) {
    metrics_util::LogSyncSigninPromoUserAction(sign_in_promo_dismissal_reason_);
    switch (sign_in_promo_dismissal_reason_) {
      case password_manager::metrics_util::CHROME_SIGNIN_OK:
        UMA_HISTOGRAM_COUNTS_100("PasswordManager.SignInPromoCountTilSignIn",
                                 sign_in_promo_shown_count);
        break;
      case password_manager::metrics_util::CHROME_SIGNIN_CANCEL:
        UMA_HISTOGRAM_COUNTS_100("PasswordManager.SignInPromoCountTilNoThanks",
                                 sign_in_promo_shown_count);
        break;
      case password_manager::metrics_util::CHROME_SIGNIN_DISMISSED:
        UMA_HISTOGRAM_COUNTS_100("PasswordManager.SignInPromoDismissalCount",
                                 sign_in_promo_shown_count);
        break;
      case password_manager::metrics_util::CHROME_SIGNIN_ACTION_COUNT:
        NOTREACHED();
        break;
    }
  } else if (model->state() ==
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    metrics_util::LogUpdateUIDismissalReason(dismissal_reason_);
  } else if (model->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    metrics_util::LogSaveUIDismissalReason(dismissal_reason_);
  } else {
    metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  }

  // Update the delegate so that it can send votes to the server.
  if (model->state() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
      model->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    // Send a notification if there was no interaction with the bubble.
    bool no_interaction =
        dismissal_reason_ == metrics_util::NO_DIRECT_INTERACTION;
    if (no_interaction && model->delegate_) {
      model->delegate_->OnNoInteraction();
    }
  }

  // Record UKM statistics on dismissal reason.
  if (model->metrics_recorder_)
    model->metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
}

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    DisplayReason display_reason)
    : delegate_(std::move(delegate)),
      interaction_reported_(false),
      are_passwords_revealed_when_bubble_is_opened_(false),
      metrics_recorder_(delegate_->GetPasswordFormMetricsRecorder()) {
  origin_ = delegate_->GetOrigin();
  state_ = delegate_->GetState();
  password_manager::InteractionsStats interaction_stats;
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    pending_password_ = delegate_->GetPendingPassword();
    local_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
    if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
      interaction_stats.origin_domain = origin_.GetOrigin();
      interaction_stats.username_value = pending_password_.username_value;
      const password_manager::InteractionsStats* stats =
          delegate_->GetCurrentInteractionStats();
      if (stats) {
        DCHECK_EQ(interaction_stats.username_value, stats->username_value);
        DCHECK_EQ(interaction_stats.origin_domain, stats->origin_domain);
        interaction_stats.dismissal_count = stats->dismissal_count;
      }
    }

    if (delegate_->ArePasswordsRevealedWhenBubbleIsOpened()) {
      are_passwords_revealed_when_bubble_is_opened_ = true;
      delegate_->OnPasswordsRevealed();
    }
    // The condition for the password reauth:
    // If the bubble opened after reauth -> no more reauth necessary, otherwise
    // If a password was autofilled -> require reauth to view it, otherwise
    // Require reauth iff the user opened the bubble manually and it's not the
    // manual saving state. The manual saving state as well as automatic prompt
    // are temporary states, therefore, it's better for the sake of convinience
    // for the user not to break the UX with the reauth prompt.
    password_revealing_requires_reauth_ =
        !are_passwords_revealed_when_bubble_is_opened_ &&
        (pending_password_.form_has_autofilled_value ||
         (!delegate_->BubbleIsManualFallbackForSaving() &&
          display_reason == USER_ACTION));
    enable_editing_ = delegate_->GetCredentialSource() !=
                      password_manager::metrics_util::CredentialSourceType::
                          kCredentialManagementAPI;

    UpdatePendingStateTitle();
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    title_ =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE);
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    pending_password_ = delegate_->GetPendingPassword();
  } else if (state_ == password_manager::ui::MANAGE_STATE) {
    local_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
    UpdateManageStateTitle();
    // TODO(pbos): Remove manage_link_ + accessors when the cocoa dialog goes
    // away. This temporarily uses the button label which is equivalent with
    // the previous link.
    manage_link_ =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON);
  }

  if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    base::string16 link = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_LINK);

    size_t offset;
    save_confirmation_text_ = l10n_util::GetStringFUTF16(
        IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT, link, &offset);
    save_confirmation_link_range_ = gfx::Range(offset, offset + link.length());
  }

  password_manager::metrics_util::UIDisplayDisposition display_disposition =
      metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
  if (display_reason == USER_ACTION) {
    switch (state_) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        display_disposition = metrics_util::MANUAL_WITH_PASSWORD_PENDING;
        break;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        display_disposition =
            metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE;
        break;
      case password_manager::ui::MANAGE_STATE:
        display_disposition = metrics_util::MANUAL_MANAGE_PASSWORDS;
        break;
      case password_manager::ui::CONFIRMATION_STATE:
        display_disposition =
            metrics_util::MANUAL_GENERATED_PASSWORD_CONFIRMATION;
        break;
      case password_manager::ui::CREDENTIAL_REQUEST_STATE:
      case password_manager::ui::AUTO_SIGNIN_STATE:
      case password_manager::ui::CHROME_SIGN_IN_PROMO_STATE:
      case password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE:
      case password_manager::ui::INACTIVE_STATE:
        NOTREACHED();
        break;
    }
  } else {
    switch (state_) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        display_disposition = metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
        break;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        display_disposition =
            metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE;
        break;
      case password_manager::ui::CONFIRMATION_STATE:
        display_disposition =
            metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION;
        break;
      case password_manager::ui::AUTO_SIGNIN_STATE:
        display_disposition = metrics_util::AUTOMATIC_SIGNIN_TOAST;
        break;
      case password_manager::ui::MANAGE_STATE:
      case password_manager::ui::CREDENTIAL_REQUEST_STATE:
      case password_manager::ui::CHROME_SIGN_IN_PROMO_STATE:
      case password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE:
      case password_manager::ui::INACTIVE_STATE:
        NOTREACHED();
        break;
    }
  }

  if (metrics_recorder_) {
    metrics_recorder_->RecordPasswordBubbleShown(
        delegate_->GetCredentialSource(), display_disposition);
  }
  metrics_util::LogUIDisplayDisposition(display_disposition);
  interaction_keeper_.reset(new InteractionKeeper(std::move(interaction_stats),
                                                  display_disposition));

  delegate_->OnBubbleShown();
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {
  if (!interaction_reported_)
    OnBubbleClosing();
}

void ManagePasswordsBubbleModel::OnBubbleClosing() {
  interaction_keeper_->ReportInteractions(this);
  if (delegate_)
    delegate_->OnBubbleHidden();
  delegate_.reset();
  interaction_reported_ = true;
}

void ManagePasswordsBubbleModel::OnNopeUpdateClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE, state_);
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_CANCEL);
  if (delegate_)
    delegate_->OnNopeUpdateClicked();
}

void ManagePasswordsBubbleModel::OnNeverForThisSiteClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_NEVER);
  if (delegate_) {
    CleanStatisticsForSite(GetProfile(), origin_);
    delegate_->NeverSavePassword();
  }
}

void ManagePasswordsBubbleModel::OnCredentialEdited(
    base::string16 new_username,
    base::string16 new_password) {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  pending_password_.username_value = std::move(new_username);
  pending_password_.password_value = std::move(new_password);
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_SAVE);
  if (delegate_) {
    CleanStatisticsForSite(GetProfile(), origin_);
    delegate_->SavePassword(pending_password_.username_value,
                            pending_password_.password_value);
  }
}

void ManagePasswordsBubbleModel::OnManageClicked() {
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_MANAGE);
  if (delegate_)
    delegate_->NavigateToPasswordManagerSettingsPage();
}

void ManagePasswordsBubbleModel::
    OnNavigateToPasswordManagerAccountDashboardLinkClicked() {
  interaction_keeper_->set_dismissal_reason(
      metrics_util::CLICKED_PASSWORDS_DASHBOARD);
  if (delegate_)
    delegate_->NavigateToPasswordManagerAccountDashboard();
}

void ManagePasswordsBubbleModel::OnAutoSignInToastTimeout() {
  interaction_keeper_->set_dismissal_reason(
      metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT);
}

void ManagePasswordsBubbleModel::OnPasswordAction(
    const autofill::PasswordForm& password_form,
    PasswordAction action) {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS).get();
  DCHECK(password_store);
  if (action == REMOVE_PASSWORD)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
}

void ManagePasswordsBubbleModel::OnSignInToChromeClicked(
    const AccountInfo& account,
    bool is_default_promo_account) {
  // Enabling sync for an existing account and starting a new sign-in are
  // triggered by the user interacting with the sign-in promo.
  interaction_keeper_->set_sign_in_promo_dismissal_reason(
      metrics_util::CHROME_SIGNIN_OK);
  GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
  if (delegate_)
    delegate_->EnableSync(account, is_default_promo_account);
}

void ManagePasswordsBubbleModel::OnSkipSignInClicked() {
  interaction_keeper_->set_sign_in_promo_dismissal_reason(
      metrics_util::CHROME_SIGNIN_CANCEL);
  GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
}

Profile* ManagePasswordsBubbleModel::GetProfile() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

content::WebContents* ManagePasswordsBubbleModel::GetWebContents() const {
  return delegate_ ? delegate_->GetWebContents() : nullptr;
}

bool ManagePasswordsBubbleModel::IsCurrentStateUpdate() const {
  DCHECK(state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_STATE);
  return std::any_of(local_credentials_.begin(), local_credentials_.end(),
                     [this](const autofill::PasswordForm& form) {
                       return form.username_value ==
                              pending_password_.username_value;
                     });
}

bool ManagePasswordsBubbleModel::ShouldShowFooter() const {
  return (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
          state_ == password_manager::ui::PENDING_PASSWORD_STATE) &&
         IsSyncUser(GetProfile());
}

const base::string16& ManagePasswordsBubbleModel::GetCurrentUsername() const {
  return pending_password_.username_value;
}

bool ManagePasswordsBubbleModel::ReplaceToShowPromotionIfNeeded() {
  Profile* profile = GetProfile();
  if (!profile)
    return false;
  PrefService* prefs = profile->GetPrefs();
  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  // Signin promotion.
  if (password_bubble_experiment::ShouldShowChromeSignInPasswordPromo(
          prefs, sync_service)) {
    interaction_keeper_->ReportInteractions(this);
    title_ =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE);
    state_ = password_manager::ui::CHROME_SIGN_IN_PROMO_STATE;
    int show_count = prefs->GetInteger(
        password_manager::prefs::kNumberSignInPasswordPromoShown);
    show_count++;
    prefs->SetInteger(password_manager::prefs::kNumberSignInPasswordPromoShown,
                      show_count);
    interaction_keeper_->set_sign_in_promo_shown_count(show_count);
    return true;
  }
#if defined(OS_WIN)
  // Desktop to mobile promotion only enabled on windows.
  if (desktop_ios_promotion::IsEligibleForIOSPromotion(
          profile,
          desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE)) {
    interaction_keeper_->ReportInteractions(this);
    title_ = desktop_ios_promotion::GetPromoTitle(
        desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE);
    state_ = password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
    return true;
  }
#endif
  return false;
}

void ManagePasswordsBubbleModel::SetClockForTesting(base::Clock* clock) {
  interaction_keeper_->SetClockForTesting(clock);
}

bool ManagePasswordsBubbleModel::RevealPasswords() {
  bool reveal_immediately = !password_revealing_requires_reauth_ ||
                            (delegate_ && delegate_->AuthenticateUser());
  if (reveal_immediately)
    delegate_->OnPasswordsRevealed();
  return reveal_immediately;
}

void ManagePasswordsBubbleModel::UpdatePendingStateTitle() {
  PasswordTitleType type =
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE
          ? PasswordTitleType::UPDATE_PASSWORD
          : (pending_password_.federation_origin.opaque()
                 ? PasswordTitleType::SAVE_PASSWORD
                 : PasswordTitleType::SAVE_ACCOUNT);
  GetSavePasswordDialogTitleTextAndLinkRange(GetWebContents()->GetVisibleURL(),
                                             origin_, type, &title_);
}

void ManagePasswordsBubbleModel::UpdateManageStateTitle() {
  GetManagePasswordsDialogTitleText(GetWebContents()->GetVisibleURL(), origin_,
                                    !local_credentials_.empty(), &title_);
}
