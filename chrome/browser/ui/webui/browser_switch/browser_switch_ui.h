// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_SWITCH_BROWSER_SWITCH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_SWITCH_BROWSER_SWITCH_UI_H_

#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class BrowserSwitchUI;

class BrowserSwitchUIConfig
    : public content::DefaultWebUIConfig<BrowserSwitchUI> {
 public:
  BrowserSwitchUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBrowserSwitchHost) {}
};

class BrowserSwitchUI : public content::WebUIController {
 public:
  explicit BrowserSwitchUI(content::WebUI* web_ui);

  BrowserSwitchUI(const BrowserSwitchUI&) = delete;
  BrowserSwitchUI& operator=(const BrowserSwitchUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_SWITCH_BROWSER_SWITCH_UI_H_
