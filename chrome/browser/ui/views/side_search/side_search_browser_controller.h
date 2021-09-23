// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

namespace views {
class WebView;
}  // namespace views

class BrowserView;
class SidePanel;
class ToolbarButton;

// Responsible for managing the WebContents hosted in the browser's side panel
// for Side Search in addition to managing the state of the side panel itself.
class SideSearchBrowserController : public content::WebContentsObserver {
 public:
  SideSearchBrowserController(SidePanel* side_panel, BrowserView* browser_view);
  SideSearchBrowserController(const SideSearchBrowserController&) = delete;
  SideSearchBrowserController& operator=(const SideSearchBrowserController&) =
      delete;
  ~SideSearchBrowserController() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void UpdateSidePanelForContents(content::WebContents* new_contents,
                                  content::WebContents* old_contents);

  std::unique_ptr<ToolbarButton> CreateToolbarButton();

  views::WebView* web_view_for_testing() { return web_view_; }

 private:
  // Gets and sets the toggled state of the side panel. If called with
  // kSideSearchStatePerTab enabled this determines whether the side panel
  // should be open for the currently active tab.
  bool GetSidePanelToggledOpen() const;
  void SetSidePanelToggledOpen(bool toggled_open);

  // Toggles panel visibility on button press.
  void SidePanelButtonPressed();

  // Updates the `side_panel_`'s visibility and updates it to host the side
  // contents associated with the currently active tab for this browser window.
  void UpdateSidePanel();

  // The toggled state of the side panel (i.e. the state of the side panel
  // as controlled by the toolbar button).
  bool toggled_open_ = false;

  ToolbarButton* toolbar_button_;
  SidePanel* const side_panel_;
  BrowserView* const browser_view_;
  views::WebView* const web_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_
