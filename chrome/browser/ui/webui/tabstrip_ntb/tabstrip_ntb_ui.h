// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TABSTRIP_NTB_TABSTRIP_NTB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TABSTRIP_NTB_TABSTRIP_NTB_UI_H_

#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"

namespace tabstrip_ntb {

class TabStripNtbUI;

class TabStripNtbUIConfig : public DefaultTopChromeWebUIConfig<TabStripNtbUI> {
 public:
  TabStripNtbUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUITabStripNtbHost) {}

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUIController for chrome://tabstrip-ntb.top-chrome/.
class TabStripNtbUI : public TopChromeWebUIController {
 public:
  explicit TabStripNtbUI(content::WebUI* web_ui);
  TabStripNtbUI(const TabStripNtbUI&) = delete;
  TabStripNtbUI& operator=(const TabStripNtbUI&) = delete;
  ~TabStripNtbUI() override;

  static constexpr std::string_view GetWebUIName() { return "TabStripNtb"; }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace tabstrip_ntb

#endif  // CHROME_BROWSER_UI_WEBUI_TABSTRIP_NTB_TABSTRIP_NTB_UI_H_
