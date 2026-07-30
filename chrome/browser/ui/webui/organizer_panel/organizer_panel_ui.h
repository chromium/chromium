// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class OrganizerPanelUI;

class OrganizerPanelUIConfig
    : public content::DefaultWebUIConfig<OrganizerPanelUI> {
 public:
  OrganizerPanelUIConfig();
};

class OrganizerPanelUI : public content::WebUIController {
 public:
  explicit OrganizerPanelUI(content::WebUI* web_ui);
  ~OrganizerPanelUI() override;

  OrganizerPanelUI(const OrganizerPanelUI&) = delete;
  OrganizerPanelUI& operator=(const OrganizerPanelUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
