// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_controller.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"

HistoryClustersSidePanelController::HistoryClustersSidePanelController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

HistoryClustersSidePanelController::~HistoryClustersSidePanelController() =
    default;

void HistoryClustersSidePanelController::ShowJourneysSidePanel(
    const std::string& query) {
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    auto* coordinator =
        HistoryClustersSidePanelCoordinator::GetOrCreateForBrowser(browser);
    coordinator->Show(query);
  }
}
