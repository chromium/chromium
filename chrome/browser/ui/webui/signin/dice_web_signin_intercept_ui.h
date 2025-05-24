// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {
class WebUI;
}

class DiceWebSigninInterceptUI;

class DiceWebSigninInterceptUIConfig
    : public content::DefaultWebUIConfig<DiceWebSigninInterceptUI> {
 public:
  DiceWebSigninInterceptUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIDiceWebSigninInterceptHost) {}
};

class DiceWebSigninInterceptUI : public content::WebUIController {
 public:
  explicit DiceWebSigninInterceptUI(content::WebUI* web_ui);
  ~DiceWebSigninInterceptUI() override;

  DiceWebSigninInterceptUI(const DiceWebSigninInterceptUI&) = delete;
  DiceWebSigninInterceptUI& operator=(const DiceWebSigninInterceptUI&) = delete;

  // Initializes the DiceWebSigninInterceptUI.
  void Initialize(
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      base::OnceCallback<void(int)> show_widget_with_height_callback,
      base::OnceCallback<void(SigninInterceptionUserChoice)>
          completion_callback);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_UI_H_
