// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "url/gurl.h"

class NavigationCounter;
class WebUIToolbarWebView;

namespace content {
class WebContents;
}

// Base test fixture for WebUI home button / home control tests.
class WebUIHomeControlTestBase : public InProcessBrowserTest {
 public:
  WebUIHomeControlTestBase();
  ~WebUIHomeControlTestBase() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  using DragOrigin = WebUIToolbarDropSimulator::DragOrigin;

  GURL GetHomeURL();

  // Clicks the "undo" link of the set-home-page bubble.
  void PerformUndo();

  // Pins the home button, moves the active tab to a known URL and snapshots the
  // state the Expect*() helpers below compare against. Must be called before
  // any Simulate*Drop()/Expect*() call.
  void SetUpHomeButtonDropTest();

  // Drops on the home button's center point. See `WebUIToolbarDropSimulator`
  // for the kinds of drop data.
  void SimulateLinkDrop(const std::string& url, DragOrigin origin);
  bool SimulateTextDrop(const std::string& text, DragOrigin origin);
  void SimulateLinkWithTextDrop(const std::string& url, DragOrigin origin);
  void SimulateFileDrop(const base::FilePath& path, DragOrigin origin);

  // The home page pref was set to `expected` and the undo bubble is showing.
  void ExpectHomePageSetTo(const GURL& expected);

  // The active tab navigated to `expected`, the home page is untouched and no
  // undo bubble appeared.
  void ExpectNavigatedTo(const GURL& expected);

  // The active tab navigated to the default search engine's results page for
  // `query`, the home page is untouched and no undo bubble appeared.
  void ExpectSearchedFor(const std::string& query);

  // Nothing happened at all: no navigation, no home page change, no bubble.
  void ExpectDropIgnored();

  WebUIToolbarWebView* webui_toolbar_view() { return webui_toolbar_view_; }
  content::WebContents* toolbar_web_contents();
  content::WebContents* active_web_contents();

 private:
  // Spins until the set-home-page bubble is showing.
  void WaitForUndoBubble();

  void ExpectHomePageUnchanged();

  void ExpectNoUndoBubble();

  base::test::ScopedFeatureList feature_list_;

  raw_ptr<WebUIToolbarWebView> webui_toolbar_view_ = nullptr;
  // Drops on the center of the home button.
  std::unique_ptr<WebUIToolbarDropSimulator> drop_simulator_;
  std::string initial_home_page_;
  bool initial_home_page_is_ntp_ = false;
  GURL initial_tab_url_;
  std::unique_ptr<NavigationCounter> navigation_counter_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_TEST_BASE_H_
