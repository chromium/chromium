// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_UI_H_

#include <string>
#include <string_view>
#include <vector>

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class SigninReauthViewController;
class SigninReauthUI;

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

class SigninReauthUIConfig
    : public content::DefaultWebUIConfig<SigninReauthUI> {
 public:
  SigninReauthUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISigninReauthHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for the signin reauth dialog.
//
// The reauth UI currently assumes that the unconsented primary account matches
// the first account in cookies.
// It's a safe assumption only under the following conditions:
// - DICE is enabled
// - Sync in not enabled
//
// Currently this dialog is only used for account password storage opt-in that
// satisfies both of those conditions.
//
// Contact chrome-signin-team@google.com if you want to reuse this dialog for
// other reauth use-cases.
class SigninReauthUI : public content::WebUIController {
 public:
  explicit SigninReauthUI(content::WebUI* web_ui);
  ~SigninReauthUI() override;

  SigninReauthUI(const SigninReauthUI&) = delete;
  SigninReauthUI& operator=(const SigninReauthUI&) = delete;

  // Creates a WebUI message handler with the specified |controller| and adds it
  // to the web UI.
  void InitializeMessageHandlerWithReauthController(
      SigninReauthViewController* controller);

 private:
  // Adds a string resource with the given GRD |ids| to the WebUI data |source|
  // named as |name|. Also stores a reverse mapping from the localized version
  // of the string to the |ids| in order to later pass it to
  // SigninReauthHandler.
  void AddStringResource(content::WebUIDataSource* source,
                         std::string_view name,
                         int ids);

  // For consent auditing.
  std::vector<std::pair<std::string, int>> js_localized_string_to_ids_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_UI_H_
