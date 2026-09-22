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
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

class NavigationCounter;
class WebUIToolbarWebView;

namespace content {
struct DropData;
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
  enum class DragOrigin {
    // Dragged out of a web page rendered by a renderer process.
    kWebPage,
    // Dragged in from outside the browser (a text editor, a file manager...).
    kOs,
  };

  GURL GetHomeURL();

  // Clicks the "undo" link of the set-home-page bubble.
  void PerformUndo();

  // Pins the home button, moves the active tab to a known URL and snapshots the
  // state the Expect*() helpers below compare against. Must be called before
  // any Simulate*Drop()/Expect*() call.
  void SetUpHomeButtonDropTest();

  // Drop of a link, i.e. `text/uri-list` only.
  void SimulateLinkDrop(const std::string& url, DragOrigin origin);

  // Drop of selected text, i.e. `text/plain` only.
  void SimulateTextDrop(const std::string& text, DragOrigin origin);

  // Drop carrying `url` in both `text/uri-list` and `text/plain`.
  void SimulateLinkWithTextDrop(const std::string& url, DragOrigin origin);

  // Drop of an OS file, i.e. `Files` plus `DropData::filenames`.
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
  // Runs a full drag-enter, drag-over, drop sequence against the toolbar's
  // RenderWidgetHost at the home button's center point.
  void PerformDropOnHomeButton(content::DropData drop_data);

  // Spins until the set-home-page bubble is showing.
  void WaitForUndoBubble();

  void ExpectHomePageUnchanged();

  void ExpectNoUndoBubble();

  base::test::ScopedFeatureList feature_list_;

  raw_ptr<WebUIToolbarWebView> webui_toolbar_view_ = nullptr;
  // Center of the home button, in the toolbar WebView's client coordinates.
  gfx::Point home_button_client_point_;
  std::string initial_home_page_;
  bool initial_home_page_is_ntp_ = false;
  GURL initial_tab_url_;
  std::unique_ptr<NavigationCounter> navigation_counter_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_TEST_BASE_H_
