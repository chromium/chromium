// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/companion_side_panel_controller.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

namespace companion {

CompanionSidePanelController::CompanionSidePanelController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

CompanionSidePanelController::~CompanionSidePanelController() = default;

void CompanionSidePanelController::ShowCompanionSidePanel() {
  if (Browser* browser = chrome::FindBrowserWithWebContents(web_contents_)) {
    auto* coordinator =
        SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
    coordinator->Show();
  }
}

void CompanionSidePanelController::UpdateNewTabButtonState() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  BrowserView* browser_view =
      browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
  if (browser_view) {
    browser_view->side_panel_coordinator()->UpdateNewTabButtonState();
  }
}

}  // namespace companion
