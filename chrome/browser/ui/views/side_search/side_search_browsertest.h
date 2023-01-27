// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSERTEST_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSERTEST_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class MouseEvent;
}  // namespace ui

namespace views {
class Button;
}  // namespace views

class BrowserView;
class SidePanel;

// Sets up a simple SideSearchConfig for browser tests and defines several test
// helper methods.
class SideSearchBrowserTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Activates the browser tab at `index`.
  void ActivateTabAt(Browser* browser, int index);

  // Appends a new tab with `url` to the end of the tabstrip.
  void AppendTab(Browser* browser, const GURL& url);

  // Navigates the browser's currently active tab to `url`.
  void NavigateActiveTab(Browser* browser,
                         const GURL& url,
                         bool is_renderer_initiated = false);

  // Gets the browser's currently active tab contents.
  content::WebContents* GetActiveSidePanelWebContents(Browser* browser);

  // Initiates a navigation to `url` for the side panel WebContents for the
  // currently active tab. Navigation will either proceed within the side panel
  // WebContents or be redirected to the active tab's WebContents depending
  // on the navigation rules set in the SideSearchConfig.
  void NavigateActiveSideContents(Browser* browser, const GURL& url);

  // Simulates a click on the browser's side search entrypoint. Assues the
  // entrypoint is visible.
  void NotifyButtonClick(Browser* browser);

  // Simulates a click on the browser side panel's close button. Assumes an
  // open side panel.
  void NotifyCloseButtonClick(Browser* browser);

  // Simulates a click on the reading list entrypoint.
  void NotifyReadLaterButtonClick(Browser* browser);

  // Helper for retrieving the BrowserView associated with `browser`.
  BrowserView* BrowserViewFor(Browser* browser);

  // Gets the side search entrypoint for `browser`. Returns null if the button
  // doesn't exist.
  views::Button* GetSideSearchButtonFor(Browser* browser);

  // Gets the side panel entrypoint for `browser`. Returns null if the button
  // doesn't exist.
  views::Button* GetSidePanelButtonFor(Browser* browser);

  // Extract the testing of the entrypoint when the side panel is open into its
  // own method as this will vary depending on whether or not the DSE support
  // flag is enabled. For e.g. when DSE support is enabled and the side panel is
  // open the omnibox button is hidden while if DSE support is disabled the
  // toolbar button is visible.
  void TestSidePanelOpenEntrypointState(Browser* browser);

  // Gets the side panel's close button. Assumes the side panel is open.
  views::Button* GetSideButtonClosePanelFor(Browser* browser);

  // Gets the side panel view for `browser`. Virtual to allow side panel v2
  // tests to supply the framework specific view.
  // TODO(tluk): Avoid having this be virtual.
  virtual SidePanel* GetSidePanelFor(Browser* browser);

  // Gets the side panel WebContents associated with the tab at `index` in the
  // browser's tab strip.
  content::WebContents* GetSidePanelContentsFor(Browser* browser, int index);

  // This method performs the following actions on the current active tab in
  // `browser`:
  //   1. Navigates to a URL that matches the SideSearchConfig's criteria for
  //      side panel navigations.
  //   2. Navigates to a URL that does not match the SideSearchConfig's
  //      criteria for side panel navigations.
  // This will assert the expected state of the side panel entrypoint and
  // visibility of the side panel.
  void NavigateToMatchingAndNonMatchingSearchPage(Browser* browser);

  // This method navigates the current active tab in `browser` to a matching,
  // then a non-matching URL, and opens the side panel.
  void NavigateToMatchingSearchPageAndOpenSidePanel(Browser* browser);

  // Gets an embedded test server URL that matches the SideSearchConfig's
  // navigation matching criteria for side panel navigations. Each call to this
  // method will return a unique URL.
  GURL GetMatchingSearchUrl();

  // Gets an embedded test server URL that does not match the SideSearchConfig's
  // navigation matching criteria for side panel navigations.Each call to this
  // method will return a unique URL.
  GURL GetNonMatchingUrl();

  // Generates a mouse click event.
  ui::MouseEvent GetDummyEvent() const;

 private:
  // Handles embedded test server requests to ensure we return successful OK
  // responses.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSERTEST_H_
