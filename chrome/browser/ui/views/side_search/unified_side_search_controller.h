// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/view.h"

class BrowserView;

// Responsible for managing the WebContents hosted in the browser's side panel
// for Side Search. Created immediately following the creation of the tab's
// WebContents.
class UnifiedSideSearchController
    : public SideSearchTabContentsHelper::Delegate,
      public content::WebContentsObserver,
      public SidePanelEntryObserver,
      public content::WebContentsUserData<UnifiedSideSearchController> {
 public:
  explicit UnifiedSideSearchController(content::WebContents* web_contents);
  UnifiedSideSearchController(const UnifiedSideSearchController&) = delete;
  UnifiedSideSearchController& operator=(const UnifiedSideSearchController&) =
      delete;
  ~UnifiedSideSearchController() override;

  // SideSearchTabContentsHelper::Delegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void SidePanelAvailabilityChanged(bool should_close) override;
  void OpenSidePanel() override;
  void CloseSidePanel(
      absl::optional<SideSearchCloseActionType> action = absl::nullopt);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // Create a WebView to host the side search WebContents.
  std::unique_ptr<views::View> GetSideSearchView();

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  BrowserView* GetBrowserView() const;

  // Clears the side contents for the currently active tab in this browser
  // window and the view in the side search registry.
  void ClearSideContentsCache();

  // Updates side panel's availability from SideSearchTabContentsHelper.
  void UpdateSidePanel();

  void UpdateSidePanelRegistry(bool is_available);

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtrFactory<UnifiedSideSearchController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_CONTROLLER_H_
