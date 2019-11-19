// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper_delegate_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "chrome/common/url_constants.h"

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
      browser = new Browser(Browser::CreateParams(profile, true));
      chrome::AddTabAt(browser, GURL(), -1, true);
    }
    browser->window()->Show();
  }
  return browser;
}

// Converts SigninEmailConfirmationDialog::Action to
// DiceTurnSyncOnHelper::SigninChoice and invokes |callback| on it.
void OnEmailConfirmation(DiceTurnSyncOnHelper::SigninChoiceCallback callback,
                         SigninEmailConfirmationDialog::Action action) {
  DCHECK(callback) << "This function should be called only once.";
  switch (action) {
    case SigninEmailConfirmationDialog::START_SYNC:
      std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
      return;
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
      std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE);
      return;
    case SigninEmailConfirmationDialog::CLOSE:
      std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
      return;
  }
  NOTREACHED();
}

}  // namespace

DiceTurnSyncOnHelperDelegateImpl::SigninDialogDelegate::SigninDialogDelegate(
    DiceTurnSyncOnHelper::SigninChoiceCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

DiceTurnSyncOnHelperDelegateImpl::SigninDialogDelegate::
    ~SigninDialogDelegate() = default;

void DiceTurnSyncOnHelperDelegateImpl::SigninDialogDelegate::OnCancelSignin() {
  DCHECK(callback_);
  std::move(callback_).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
}

void DiceTurnSyncOnHelperDelegateImpl::SigninDialogDelegate::
    OnContinueSignin() {
  DCHECK(callback_);
  std::move(callback_).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
}

void DiceTurnSyncOnHelperDelegateImpl::SigninDialogDelegate::
    OnSigninWithNewProfile() {
  DCHECK(callback_);
  std::move(callback_).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE);
}

DiceTurnSyncOnHelperDelegateImpl::DiceTurnSyncOnHelperDelegateImpl(
    Browser* browser)
    : browser_(browser), profile_(browser_->profile()) {
  DCHECK(browser);
  DCHECK(profile_);
  BrowserList::AddObserver(this);
}

DiceTurnSyncOnHelperDelegateImpl::~DiceTurnSyncOnHelperDelegateImpl() {
  BrowserList::RemoveObserver(this);
}

void DiceTurnSyncOnHelperDelegateImpl::ShowLoginError(
    const std::string& email,
    const std::string& error_message) {
  LoginUIServiceFactory::GetForProfile(profile_)->DisplayLoginResult(
      browser_, base::UTF8ToUTF16(error_message), base::UTF8ToUTF16(email));
}

void DiceTurnSyncOnHelperDelegateImpl::ShowEnterpriseAccountConfirmation(
    const std::string& email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  DCHECK(callback);
  browser_ = EnsureBrowser(browser_, profile_);
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("Signin_Show_EnterpriseAccountPrompt"));
  TabDialogs::FromWebContents(web_contents)
      ->ShowProfileSigninConfirmation(
          browser_, profile_, email,
          std::make_unique<SigninDialogDelegate>(std::move(callback)));
}

void DiceTurnSyncOnHelperDelegateImpl::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  scoped_login_ui_service_observer_.Add(
      LoginUIServiceFactory::GetForProfile(profile_));
  browser_ = EnsureBrowser(browser_, profile_);
  browser_->signin_view_controller()->ShowModalSyncConfirmationDialog(browser_);
}

void DiceTurnSyncOnHelperDelegateImpl::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  DCHECK(callback);
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  // TODO(droger): Replace Bind with BindOnce once the
  // SigninEmailConfirmationDialog supports it.
  SigninEmailConfirmationDialog::AskForConfirmation(
      web_contents, profile_, previous_email, new_email,
      base::Bind(&OnEmailConfirmation, base::Passed(std::move(callback))));
}

void DiceTurnSyncOnHelperDelegateImpl::ShowSyncSettings() {
  browser_ = EnsureBrowser(browser_, profile_);
  chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
}

void DiceTurnSyncOnHelperDelegateImpl::SwitchToProfile(Profile* new_profile) {
  profile_ = new_profile;
  browser_ = nullptr;
}

void DiceTurnSyncOnHelperDelegateImpl::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  DCHECK(sync_confirmation_callback_);
  std::move(sync_confirmation_callback_).Run(result);
}

void DiceTurnSyncOnHelperDelegateImpl::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = nullptr;
}
