// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_

#include "build/build_config.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserWindowInterface;

// The LoginUIService helps track per-profile information for the login related
// UIs.
class LoginUIService : public KeyedService {
 public:
  LoginUIService();

  LoginUIService(const LoginUIService&) = delete;
  LoginUIService& operator=(const LoginUIService&) = delete;

  ~LoginUIService() override;

  // If `error.message()` is not empty, displays login error message in the
  // Modal Signin Error dialog of the browser associated with
  // `browser_window_features`.
  void DisplayLoginResult(BrowserWindowInterface& browser_window,
                          const SigninUIError& error);

#if !BUILDFLAG(IS_CHROMEOS)
  // Gets the last error set through |DisplayLoginResult|.
  const SigninUIError& GetLastLoginError() const;
#endif

 private:
#if !BUILDFLAG(IS_CHROMEOS)
  SigninUIError last_login_error_ = SigninUIError::Ok();
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
