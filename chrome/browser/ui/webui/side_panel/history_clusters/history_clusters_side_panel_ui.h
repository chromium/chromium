// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_

#include "ui/webui/mojo_bubble_web_ui_controller.h"

class HistoryClustersSidePanelUI : public ui::MojoBubbleWebUIController {
 public:
  explicit HistoryClustersSidePanelUI(content::WebUI* web_ui);
  HistoryClustersSidePanelUI(const HistoryClustersSidePanelUI&) = delete;
  HistoryClustersSidePanelUI& operator=(const HistoryClustersSidePanelUI&) =
      delete;
  ~HistoryClustersSidePanelUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
