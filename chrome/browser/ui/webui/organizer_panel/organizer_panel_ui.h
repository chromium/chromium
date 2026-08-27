// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_

#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/browser/webui_config.h"

class OrganizerPanelUI;

class OrganizerPanelUIConfig
    : public DefaultTopChromeWebUIConfig<OrganizerPanelUI> {
 public:
  OrganizerPanelUIConfig();
};

class OrganizerPanelUI : public TopChromeWebUIController {
 public:
  explicit OrganizerPanelUI(content::WebUI* web_ui);
  ~OrganizerPanelUI() override;

  OrganizerPanelUI(const OrganizerPanelUI&) = delete;
  OrganizerPanelUI& operator=(const OrganizerPanelUI&) = delete;

  static constexpr std::string_view GetWebUIName() { return "OrganizerPanel"; }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
