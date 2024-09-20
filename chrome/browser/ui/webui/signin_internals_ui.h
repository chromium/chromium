// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_

#include "base/values.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"

class SignInInternalsUI;

class SignInInternalsUIConfig
    : public content::DefaultWebUIConfig<SignInInternalsUI> {
 public:
  SignInInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISignInInternalsHost) {}
};

// The implementation for the chrome://signin-internals page.
class SignInInternalsUI : public content::WebUIController {
 public:
  explicit SignInInternalsUI(content::WebUI* web_ui);
  ~SignInInternalsUI() override;

  SignInInternalsUI(const SignInInternalsUI&) = delete;
  SignInInternalsUI& operator=(const SignInInternalsUI&) = delete;
};

class SignInInternalsHandler : public content::WebUIMessageHandler,
                               public AboutSigninInternals::Observer {
 public:
  SignInInternalsHandler();
  ~SignInInternalsHandler() override;

  SignInInternalsHandler(const SignInInternalsHandler&) = delete;
  SignInInternalsHandler& operator=(const SignInInternalsHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleGetSignInInfo(const base::Value::List& args);

  // AboutSigninInternals::Observer::OnSigninStateChanged implementation.
  void OnSigninStateChanged(const base::Value::Dict& info) override;

  // Notification that the cookie accounts are ready to be displayed.
  void OnCookieAccountsFetched(const base::Value::Dict& info) override;

 private:
  base::ScopedObservation<AboutSigninInternals, AboutSigninInternals::Observer>
      about_signin_internals_observeration_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_
