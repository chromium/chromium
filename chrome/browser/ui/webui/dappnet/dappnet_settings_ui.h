// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DAPPNET_DAPPNET_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DAPPNET_DAPPNET_SETTINGS_UI_H_

#include "content/public/browser/webui_config.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// The WebUI for chrome://dappnet/config  
class DappnetSettingsUI : public content::WebUIController {
 public:
  explicit DappnetSettingsUI(content::WebUI* web_ui);
  ~DappnetSettingsUI() override;

  DappnetSettingsUI(const DappnetSettingsUI&) = delete;
  DappnetSettingsUI& operator=(const DappnetSettingsUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class DappnetSettingsUIConfig : public content::DefaultWebUIConfig<DappnetSettingsUI> {
 public:
  DappnetSettingsUIConfig();
  
  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DAPPNET_DAPPNET_SETTINGS_UI_H_