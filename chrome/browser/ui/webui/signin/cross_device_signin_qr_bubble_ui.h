// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class CrossDeviceSigninQrBubbleUI;

class CrossDeviceSigninQrBubbleUIConfig
    : public content::DefaultWebUIConfig<CrossDeviceSigninQrBubbleUI> {
 public:
  CrossDeviceSigninQrBubbleUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for the cross-device QR code bubble.
class CrossDeviceSigninQrBubbleUI : public content::WebUIController {
 public:
  explicit CrossDeviceSigninQrBubbleUI(content::WebUI* web_ui);
  ~CrossDeviceSigninQrBubbleUI() override;

  CrossDeviceSigninQrBubbleUI(const CrossDeviceSigninQrBubbleUI&) = delete;
  CrossDeviceSigninQrBubbleUI& operator=(const CrossDeviceSigninQrBubbleUI&) =
      delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_UI_H_
