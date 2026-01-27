// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"

LoginUIService::LoginUIService() = default;

LoginUIService::~LoginUIService() = default;

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
  for (Observer& observer : observer_list_) {
    observer.OnSyncConfirmationUIClosed(result);
  }
}

void LoginUIService::DisplayLoginResult(
    BrowserWindowFeatures& browser_window_features,
    const SigninUIError& error) {
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS doesn't have the avatar bubble so it never calls this function.
  NOTREACHED();
#else
  last_login_error_ = error;
  // TODO(crbug.com/40225985): Check if the condition should be `!error.IsOk()`
  if (!error.message().empty()) {
    browser_window_features.signin_view_controller()
        ->ShowModalSigninErrorDialog();
  }
#endif
}

#if !BUILDFLAG(IS_CHROMEOS)
const SigninUIError& LoginUIService::GetLastLoginError() const {
  return last_login_error_;
}
#endif
