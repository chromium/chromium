// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_tab_helper.h"

namespace content {
class WebContents;
}  // namespace content

class HistoryClustersSidePanelController
    : public side_panel::HistoryClustersTabHelper::Delegate {
 public:
  explicit HistoryClustersSidePanelController(
      content::WebContents* web_contents);
  HistoryClustersSidePanelController(
      const HistoryClustersSidePanelController&) = delete;
  HistoryClustersSidePanelController& operator=(
      const HistoryClustersSidePanelController&) = delete;
  ~HistoryClustersSidePanelController() override;

  // HistoryClustersTabHelper::Delegate:
  void ShowJourneysSidePanel(const std::string& query) override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_CONTROLLER_H_
