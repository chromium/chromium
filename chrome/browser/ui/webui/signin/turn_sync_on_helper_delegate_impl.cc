// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_delegate_impl.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/url_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
void OnEmailConfirmation(TurnSyncOnHelper::SigninChoiceCallback callback,
                         SigninEmailConfirmationDialog::Action action) {
  DCHECK(callback) << "This function should be called only once.";
  switch (action) {
    case SigninEmailConfirmationDialog::START_SYNC:
      std::move(callback).Run(TurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
      return;
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
      std::move(callback).Run(TurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE);
      return;
    case SigninEmailConfirmationDialog::CLOSE:
      std::move(callback).Run(TurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
      return;
  }
  NOTREACHED();
}

void OnProfileCheckComplete(const AccountInfo& account_info,
                            TurnSyncOnHelper::SigninChoiceCallback callback,
                            base::WeakPtr<Browser> browser,
                            bool prompt_for_new_profile) {
  if (!browser) {
    std::move(callback).Run(TurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
    return;
  }
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(browser->profile()->GetPath());
  browser->signin_view_controller()->ShowModalEnterpriseConfirmationDialog(
      account_info, GenerateNewProfileColor(entry).color,
      base::BindOnce(
          [](TurnSyncOnHelper::SigninChoiceCallback callback, Browser* browser,
             bool prompt_for_new_profile, bool create_profile) {
            browser->signin_view_controller()->CloseModalSignin();
            std::move(callback).Run(
                create_profile
                    ? prompt_for_new_profile
                          ? TurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE
                          : TurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE
                    : TurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
          },
          std::move(callback), browser.get(), prompt_for_new_profile));
}

}  // namespace

TurnSyncOnHelperDelegateImpl::TurnSyncOnHelperDelegateImpl(Browser* browser)
    : browser_(browser), profile_(browser_->profile()) {
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
    TurnSyncOnHelper::SigninChoiceCallback callback) {
  browser_ = EnsureBrowser(browser_, profile_);
  // Checking whether to show the prompt for a new profile is sometimes
  // asynchronous.
  ShouldEnterpriseConfirmationPromptForNewProfile(
      profile_, base::BindOnce(&OnProfileCheckComplete, account_info,
                               std::move(callback), browser_->AsWeakPtr()));
}

void TurnSyncOnHelperDelegateImpl::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));
  browser_ = EnsureBrowser(browser_, profile_);
  browser_->signin_view_controller()->ShowModalSyncConfirmationDialog();
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
    TurnSyncOnHelper::SigninChoiceCallback callback) {
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
