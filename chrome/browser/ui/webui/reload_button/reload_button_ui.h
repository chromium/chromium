// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_UI_H_
#define CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_UI_H_

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class ReloadButtonUI;

class ReloadButtonUI : public TopChromeWebUIController {
 public:
  explicit ReloadButtonUI(content::WebUI* web_ui);
  ~ReloadButtonUI() override;
  static constexpr std::string_view GetWebUIName() { return "ReloadButtonUI"; }
};

class ReloadButtonUIConfig
    : public DefaultTopChromeWebUIConfig<ReloadButtonUI> {
 public:
  ReloadButtonUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUIReloadButtonHost) {}
  // DefaultTopChromeWebUIConfig overrides:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_UI_H_
