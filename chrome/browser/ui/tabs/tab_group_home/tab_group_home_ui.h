// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_HOME_TAB_GROUP_HOME_UI_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_HOME_TAB_GROUP_HOME_UI_H_

#include "chrome/browser/ui/tabs/tab_group_home/constants.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace tabs {

class TabGroupHomeUI : public TopChromeWebUIController {
 public:
  explicit TabGroupHomeUI(content::WebUI* web_ui);
  TabGroupHomeUI(const TabGroupHomeUI&) = delete;
  TabGroupHomeUI& operator=(const TabGroupHomeUI&) = delete;
  ~TabGroupHomeUI() override;

  static constexpr std::string GetWebUIName() { return "TabGroupHome"; }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

class TabGroupHomeUIConfig
    : public DefaultTopChromeWebUIConfig<TabGroupHomeUI> {
 public:
  TabGroupHomeUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    kTabGroupHomeHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_HOME_TAB_GROUP_HOME_UI_H_
