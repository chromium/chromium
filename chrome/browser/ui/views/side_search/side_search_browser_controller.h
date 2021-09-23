// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_

namespace views {
class WebView;
}  // namespace views

class BrowserView;
class SidePanel;

// Responsible for managing the WebContents hosted in the browser's side panel
// for Side Search in addition to managing the state of the side panel itself.
class SideSearchBrowserController {
 public:
  SideSearchBrowserController(SidePanel* side_panel, BrowserView* browser_view);
  SideSearchBrowserController(const SideSearchBrowserController&) = delete;
  SideSearchBrowserController& operator=(const SideSearchBrowserController&) =
      delete;
  virtual ~SideSearchBrowserController();

 private:
  // Updates the `side_panel_`'s visibility and updates it to host the side
  // contents associated with the currently active tab for this browser window.
  void UpdateSidePanel();

  SidePanel* const side_panel_;
  BrowserView* const browser_view_;
  views::WebView* const web_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_
