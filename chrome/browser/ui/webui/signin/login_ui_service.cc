// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"

LoginUIService::LoginUIService() = default;

LoginUIService::~LoginUIService() = default;

void LoginUIService::DisplayLoginResult(BrowserWindowInterface& browser_window,
                                        const SigninUIError& error) {
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS doesn't have the avatar bubble so it never calls this function.
  NOTREACHED();
#else
  last_login_error_ = error;
  // TODO(crbug.com/40225985): Check if the condition should be `!error.IsOk()`
  if (!error.message().empty()) {
    SigninViewController::From(&browser_window)->ShowModalSigninErrorDialog();
  }
#endif
}

#if !BUILDFLAG(IS_CHROMEOS)
const SigninUIError& LoginUIService::GetLastLoginError() const {
  return last_login_error_;
}
#endif
