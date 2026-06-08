// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_QRCODE_BAR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_QRCODE_BAR_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class SigninQRCodeBarUI : public content::WebUIController {
 public:
  explicit SigninQRCodeBarUI(content::WebUI* web_ui);
  SigninQRCodeBarUI(const SigninQRCodeBarUI&) = delete;
  SigninQRCodeBarUI& operator=(const SigninQRCodeBarUI&) = delete;
  ~SigninQRCodeBarUI() override;
};

class SigninQRCodeBarUIConfig
    : public content::DefaultWebUIConfig<SigninQRCodeBarUI> {
 public:
  SigninQRCodeBarUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISigninQRCodeBarHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_QRCODE_BAR_UI_H_
