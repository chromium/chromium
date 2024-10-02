// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_delegate_impl.h"

#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"
#include "components/policy/core/common/policy_utils.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

// If the |browser| argument is non-null, returns the pointer directly.
// Otherwise creates a new browser for the profile, adds an empty tab and makes
// sure the browser is visible.
Browser* EnsureBrowser(Browser* browser, Profile* profile) {
  if (!browser) {
    // The user just created a new profile or has closed the browser that
    // we used previously. Grab the most recently active browser or else
    // create a new one.
    browser = chrome::FindLastActiveWithProfile(profile);
    if (!browser) {
      browser = Browser::Create(Browser::CreateParams(profile, true));
      chrome::AddTabAt(browser, GURL(), -1, true);
    }
    browser->window()->Show();
  }
  return browser;
}

// Converts SigninEmailConfirmationDialog::Action to
// TurnSyncOnHelper::SigninChoice and invokes |callback| on it.
void OnEmailConfirmation(signin::SigninChoiceCallback callback,
                         SigninEmailConfirmationDialog::Action action) {
  DCHECK(callback) << "This function should be called only once.";
  switch (action) {
    case SigninEmailConfirmationDialog::START_SYNC:
      std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
      return;
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
      std::move(callback).Run(signin::SIGNIN_CHOICE_NEW_PROFILE);
      return;
    case SigninEmailConfirmationDialog::CLOSE:
      std::move(callback).Run(signin::SIGNIN_CHOICE_CANCEL);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

TurnSyncOnHelperDelegateImpl::TurnSyncOnHelperDelegateImpl(Browser* browser,
                                                           bool is_sync_promo)
    : browser_(browser),
      profile_(browser_->profile()),
      is_sync_promo_(is_sync_promo) {
  DCHECK(browser);
  DCHECK(profile_);
  BrowserList::AddObserver(this);
}

TurnSyncOnHelperDelegateImpl::~TurnSyncOnHelperDelegateImpl() {
  BrowserList::RemoveObserver(this);
}

void TurnSyncOnHelperDelegateImpl::ShowLoginError(const SigninUIError& error) {
  DCHECK(!error.IsOk());
  TurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser(error, browser_);
}

void TurnSyncOnHelperDelegateImpl::
    ShouldEnterpriseConfirmationPromptForNewProfile(
        Profile* profile,
        base::OnceCallback<void(bool)> callback) {
  ui::CheckShouldPromptForNewProfile(profile, std::move(callback));
}

void TurnSyncOnHelperDelegateImpl::ShowEnterpriseAccountConfirmation(
    const AccountInfo& account_info,
    signin::SigninChoiceCallback callback) {
  browser_ = EnsureBrowser(browser_, profile_);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Profile Separation Enforced is not supported on Lacros.
  OnProfileCheckComplete(account_info, std::move(callback),
                         /*prompt_for_new_profile=*/false);
#else
  account_level_signin_restriction_policy_fetcher_ =
      std::make_unique<policy::UserCloudSigninRestrictionPolicyFetcher>(
          g_browser_process->browser_policy_connector(),
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory());
  // Checking whether to show the prompt for a new profile is sometimes
  // asynchronous.
  ShouldEnterpriseConfirmationPromptForNewProfile(
      profile_,
      base::BindOnce(&TurnSyncOnHelperDelegateImpl::OnProfileCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), account_info,
                     std::move(callback)));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void TurnSyncOnHelperDelegateImpl::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));
  browser_ = EnsureBrowser(browser_, profile_);
  browser_->signin_view_controller()->ShowModalSyncConfirmationDialog(
      /*is_signin_intercept=*/false, is_sync_promo_);
}

void TurnSyncOnHelperDelegateImpl::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  // This is handled by the same UI element as the normal sync confirmation.
  ShowSyncConfirmation(std::move(callback));
}

void TurnSyncOnHelperDelegateImpl::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    signin::SigninChoiceCallback callback) {
  DCHECK(callback);
  browser_ = EnsureBrowser(browser_, profile_);
  browser_->signin_view_controller()->ShowModalSigninEmailConfirmationDialog(
      previous_email, new_email,
      base::BindOnce(&OnEmailConfirmation, std::move(callback)));
}

void TurnSyncOnHelperDelegateImpl::ShowSyncSettings() {
  browser_ = EnsureBrowser(browser_, profile_);
  chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
}

void TurnSyncOnHelperDelegateImpl::SwitchToProfile(Profile* new_profile) {
  profile_ = new_profile;
  browser_ = nullptr;
}

void TurnSyncOnHelperDelegateImpl::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  DCHECK(sync_confirmation_callback_);
  // Treat closing the ui as an implicit ABORT_SYNC action.
  if (result == LoginUIService::UI_CLOSED)
    result = LoginUIService::ABORT_SYNC;
  if (browser_)
    browser_->signin_view_controller()->CloseModalSignin();
  std::move(sync_confirmation_callback_).Run(result);
}

void TurnSyncOnHelperDelegateImpl::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = nullptr;
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
void TurnSyncOnHelperDelegateImpl::OnProfileSigninRestrictionsFetched(
    const AccountInfo& account_info,
    signin::SigninChoiceCallback callback,
    const policy::ProfileSeparationPolicies& profile_separation_policies) {
  if (!browser_) {
    std::move(callback).Run(signin::SIGNIN_CHOICE_CANCEL);
    return;
  }
  auto profile_creation_required_by_policy =
      signin_util::IsProfileSeparationEnforcedByProfile(browser_->profile(),
                                                        account_info.email) ||
      signin_util::IsProfileSeparationEnforcedByPolicies(
          profile_separation_policies);
  bool show_link_data_option = signin_util::
      ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
          browser_->profile(), profile_separation_policies);
  browser_->signin_view_controller()->ShowModalManagedUserNoticeDialog(
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info, /*is_oidc_account=*/false,
          profile_creation_required_by_policy, show_link_data_option,
          std::move(callback),
          base::BindOnce(&SigninViewController::CloseModalSignin,
                         browser_->signin_view_controller()->AsWeakPtr())));
}
#endif

void TurnSyncOnHelperDelegateImpl::OnProfileCheckComplete(
    const AccountInfo& account_info,
    signin::SigninChoiceCallback callback,
    bool prompt_for_new_profile) {
  if (!browser_) {
    std::move(callback).Run(signin::SIGNIN_CHOICE_CANCEL);
    return;
  }
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (prompt_for_new_profile) {
    account_level_signin_restriction_policy_fetcher_
        ->GetManagedAccountsSigninRestriction(
            IdentityManagerFactory::GetForProfile(browser_->profile()),
            account_info.account_id,
            base::BindOnce(&TurnSyncOnHelperDelegateImpl::
                               OnProfileSigninRestrictionsFetched,
                           weak_ptr_factory_.GetWeakPtr(), account_info,
                           std::move(callback)),
            policy::utils::IsPolicyTestingEnabled(
                browser_->profile()->GetPrefs(), chrome::GetChannel())
                ? browser_->profile()
                      ->GetPrefs()
                      ->GetDefaultPrefValue(
                          prefs::
                              kUserCloudSigninPolicyResponseFromPolicyTestPage)
                      ->GetString()
                : std::string());
    return;
  }
#endif
  DCHECK(!prompt_for_new_profile);
  browser_->signin_view_controller()->ShowModalManagedUserNoticeDialog(
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info, /*is_oidc_account=*/false,
          /*profile_creation_required_by_policy=*/false,
          /*show_link_data_option=*/false,
          base::BindOnce(
              [](signin::SigninChoiceCallback callback,
                 signin::SigninChoice choice) {
                // When `show_link_data_option` is false,
                // `ShowModalManagedUserNoticeDialog()` calls back
                // with either `SIGNIN_CHOICE_CANCEL` or
                // `SIGNIN_CHOICE_NEW_PROFILE`. The profile is clean here, no
                // need to create a new one.
                std::move(callback).Run(
                    choice == signin::SigninChoice::SIGNIN_CHOICE_CANCEL
                        ? signin::SigninChoice::SIGNIN_CHOICE_CANCEL
                        : signin::SigninChoice::SIGNIN_CHOICE_CONTINUE);
              },
              std::move(callback)),
          base::BindOnce(&SigninViewController::CloseModalSignin,
                         browser_->signin_view_controller()->AsWeakPtr())));
}
