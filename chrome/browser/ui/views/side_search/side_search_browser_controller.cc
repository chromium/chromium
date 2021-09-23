// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "ui/views/controls/webview/webview.h"

SideSearchBrowserController::SideSearchBrowserController(
    SidePanel* side_panel,
    BrowserView* browser_view)
    : side_panel_(side_panel),
      browser_view_(browser_view),
      web_view_(side_panel_->AddChildView(
          std::make_unique<views::WebView>(browser_view_->GetProfile()))) {
  // TODO(tluk): Add necessary control logic.
  side_panel_->SetVisible(true);
  UpdateSidePanel();
}

SideSearchBrowserController::~SideSearchBrowserController() = default;

void SideSearchBrowserController::UpdateSidePanel() {
  auto* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents)
    return;

  // Switch the WebContents currently in the windows side panel to the
  // WebContents associated with the active tab.
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);

  // The side panel contents will be created if it does not already exist.
  web_view_->SetWebContents(tab_contents_helper->GetSidePanelContents());
}
