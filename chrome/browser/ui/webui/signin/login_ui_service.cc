// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/user_manager.h"
#endif  // !defined(OS_CHROMEOS)

LoginUIService::LoginUIService(Profile* profile)
#if !defined(OS_CHROMEOS)
    : profile_(profile)
#endif
{
}

LoginUIService::~LoginUIService() {}

void LoginUIService::AddObserver(LoginUIService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LoginUIService::RemoveObserver(LoginUIService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

LoginUIService::LoginUI* LoginUIService::current_login_ui() const {
  return ui_list_.empty() ? nullptr : ui_list_.front();
}

void LoginUIService::SetLoginUI(LoginUI* ui) {
  ui_list_.remove(ui);
  ui_list_.push_front(ui);
}

void LoginUIService::LoginUIClosed(LoginUI* ui) {
  ui_list_.remove(ui);
}

void LoginUIService::SyncConfirmationUIClosed(
    SyncConfirmationUIClosedResult result) {
  for (Observer& observer : observer_list_)
    observer.OnSyncConfirmationUIClosed(result);
}

void LoginUIService::ShowExtensionLoginPrompt(bool enable_sync,
                                              const std::string& email_hint) {
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  // There is no sign-in flow for guest or system profile.
  if (profile_->IsGuestSession() || profile_->IsSystemProfile())
    return;
  // Locked profile should be unlocked with UserManager only.
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry) &&
      entry->IsSigninRequired()) {
    return;
  }

  // This may be called in incognito. Redirect to the original profile.
  chrome::ScopedTabbedBrowserDisplayer displayer(
      profile_->GetOriginalProfile());
  Browser* browser = displayer.browser();

  if (enable_sync) {
    // Set a primary account.
    browser->signin_view_controller()->ShowDiceEnableSyncTab(
        browser, signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO, email_hint);
  } else {
    // Add an account to the web without setting a primary account.
    browser->signin_view_controller()->ShowDiceAddAccountTab(
        browser, signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
        email_hint);
  }
#endif
}

void LoginUIService::DisplayLoginResult(Browser* browser,
                                        const base::string16& error_message,
                                        const base::string16& email) {
#if defined(OS_CHROMEOS)
  // ChromeOS doesn't have the avatar bubble so it never calls this function.
  NOTREACHED();
#else
  is_displaying_profile_blocking_error_message_ = false;
  last_login_result_ = error_message;
  last_login_error_email_ = email;
  if (!error_message.empty()) {
    if (browser)
      browser->signin_view_controller()->ShowModalSigninErrorDialog(browser);
    else
      UserManagerProfileDialog::DisplayErrorMessage();
  } else if (browser) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        BrowserWindow::AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN,

        signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS, false);
  }
#endif
}

void LoginUIService::SetProfileBlockingErrorMessage() {
  last_login_result_ = base::string16();
  last_login_error_email_ = base::string16();
  is_displaying_profile_blocking_error_message_ = true;
}

bool LoginUIService::IsDisplayingProfileBlockedErrorMessage() const {
  return is_displaying_profile_blocking_error_message_;
}

const base::string16& LoginUIService::GetLastLoginResult() const {
  return last_login_result_;
}

const base::string16& LoginUIService::GetLastLoginErrorEmail() const {
  return last_login_error_email_;
}
