// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/views/controls/webview/webview.h"

SideSearchBrowserController::SideSearchBrowserController(
    SidePanel* side_panel,
    BrowserView* browser_view)
    : side_panel_(side_panel),
      browser_view_(browser_view),
      web_view_(side_panel_->AddChildView(
          std::make_unique<views::WebView>(browser_view_->GetProfile()))) {
  UpdateSidePanelForContents(browser_view_->GetActiveWebContents(), nullptr);
}

SideSearchBrowserController::~SideSearchBrowserController() {
  Observe(nullptr);
}

void SideSearchBrowserController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // We need to update the side panel state in response to navigations to catch
  // cases where the user navigates to a page that should have the side panel
  // hidden (e.g. the Google home page).
  UpdateSidePanel();
}

void SideSearchBrowserController::UpdateSidePanelForContents(
    content::WebContents* new_contents,
    content::WebContents* old_contents) {
  Observe(new_contents);

  // Update the state of the side panel to catch cases where we switch to a tab
  // where the panel should be hidden (or vise versa).
  UpdateSidePanel();
}

bool SideSearchBrowserController::GetSidePanelToggledOpen() const {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab)) {
    auto* active_contents = browser_view_->GetActiveWebContents();
    return active_contents
               ? SideSearchTabContentsHelper::FromWebContents(active_contents)
                     ->toggled_open()
               : false;
  }
  return toggled_open_;
}

void SideSearchBrowserController::SetSidePanelToggledOpen(bool toggled_open) {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab)) {
    if (auto* active_contents = browser_view_->GetActiveWebContents()) {
      SideSearchTabContentsHelper::FromWebContents(active_contents)
          ->set_toggled_open(toggled_open);
    }
    return;
  }
  toggled_open_ = toggled_open;
}

void SideSearchBrowserController::UpdateSidePanel() {
  auto* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents) {
    // Ensure we reset the `web_view_`'s hosted side contents when the active
    // tab contents is null to cover cases such as the tab being moved to
    // another window. This is needed as the WebView's destructor will not be
    // invoked until both the remove model update is fired in this window and
    // the add model update is fired in the destination window.
    web_view_->SetWebContents(nullptr);
    return;
  }

  // Switch the WebContents currently in the windows side panel to the
  // WebContents associated with the active tab.
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);

  const bool can_show_side_panel_for_page =
      tab_contents_helper->CanShowSidePanelForCommittedNavigation();
  const bool will_show_side_panel =
      can_show_side_panel_for_page && GetSidePanelToggledOpen();

  // The side panel contents will be created if it does not already exist.
  web_view_->SetWebContents(will_show_side_panel
                                ? tab_contents_helper->GetSidePanelContents()
                                : nullptr);
  side_panel_->SetVisible(will_show_side_panel);
}
