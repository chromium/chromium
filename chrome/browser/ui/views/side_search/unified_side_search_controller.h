// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_search/default_search_icon_source.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/view.h"

class BrowserView;
class Profile;

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
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void SidePanelAvailabilityChanged(bool should_close) override;
  void OpenSidePanel() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  base::WeakPtr<UnifiedSideSearchController> GetWeakPtr();
  void CloseSidePanel();

  // Gets the URL needed to open the current side search side panel contents
  // into a new tab.
  GURL GetOpenInNewTabURL() const;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  BrowserView* GetBrowserView() const;
  Profile* GetProfile() const;
  SidePanelUI* GetSidePanelUI();

  // Create a WebView to host the side search WebContents.
  std::unique_ptr<views::View> GetSideSearchView();

  // Creates a ImageModel for the current DSE's favicon.
  ui::ImageModel GetSideSearchIcon();

  // Creates a string representing the side search side panel's hosted content.
  std::u16string GetSideSearchName() const;

  // Updates side panel's availability from SideSearchTabContentsHelper.
  void UpdateSidePanel();

  void UpdateSidePanelRegistry(bool is_available);

  // True if the side panel should be automatically triggered after a navigation
  // defined by `navigation_handle`.
  bool ShouldAutomaticallyTriggerAfterNavigation(
      content::NavigationHandle* navigation_handle);

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtrFactory<UnifiedSideSearchController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_CONTROLLER_H_
