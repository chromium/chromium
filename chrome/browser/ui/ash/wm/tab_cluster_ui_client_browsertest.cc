// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

// The title of chrome/test/data/title2.html.
constexpr char kTitle2[] = "Title Of Awesomeness";

class TabClusterUIClientBrowserTest : public InProcessBrowserTest {
 public:
  TabClusterUIClientBrowserTest() = default;
  TabClusterUIClientBrowserTest(const TabClusterUIClientBrowserTest&) = delete;
  TabClusterUIClientBrowserTest& operator=(
      const TabClusterUIClientBrowserTest&) = delete;
  ~TabClusterUIClientBrowserTest() override = default;

 protected:
  content::WebContents* GetActiveWebContents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  // Returns the info of the tab item for the only tab in `browser()`.
  const ash::TabClusterUIItem::Info& GetInfo() {
    aura::Window* window = browser()->GetWindow()->GetNativeWindow();
    const ash::TabClusterUIItem* found = nullptr;
    for (const auto& item :
         ash::Shell::Get()->tab_cluster_ui_controller()->tab_items()) {
      if (item->current_info().browser_window == window) {
        CHECK(!found);
        found = item.get();
      }
    }
    CHECK(found);
    return found->current_info();
  }

  // Navigates the active tab to title2.html and waits for the tab item to
  // reflect it.
  void NavigateToTitle2() {
    const GURL url = embedded_test_server()->GetURL("/title2.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(base::test::RunUntil([&] {
      return GetInfo().source == url.spec() && GetInfo().title == kTitle2 &&
             !GetInfo().is_loading;
    }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kCoralFeature};
};

// Tests that title changes and same-document navigations are reflected.
IN_PROC_BROWSER_TEST_F(TabClusterUIClientBrowserTest,
                       TitleChangeAndSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(NavigateToTitle2());
  content::WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::ExecJs(web_contents, "document.title = 'New title';"));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return GetInfo().title == "New title"; }));

  const GURL pushed_url = embedded_test_server()->GetURL("/pushed.html");
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "history.pushState({}, '', '/pushed.html');"));
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return GetInfo().source == pushed_url.spec(); }));
  EXPECT_FALSE(GetInfo().is_loading);
}

// Tests that the URL of a pending navigation is not reported, and that the tab
// item is updated once the navigation commits and finishes loading.
IN_PROC_BROWSER_TEST_F(TabClusterUIClientBrowserTest, PendingNavigation) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/slow");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(NavigateToTitle2());
  const GURL url = embedded_test_server()->GetURL("/title2.html");
  content::WebContents* web_contents = GetActiveWebContents();

  // Start a browser-initiated navigation, whose URL becomes visible right away,
  // and stall it before it commits.
  const GURL slow_url = embedded_test_server()->GetURL("/slow");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), slow_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  response.WaitForRequest();
  ASSERT_EQ(web_contents->GetVisibleURL(), slow_url);
  EXPECT_TRUE(base::test::RunUntil([&] { return GetInfo().is_loading; }));
  EXPECT_EQ(GetInfo().source, url.spec());
  EXPECT_EQ(GetInfo().title, kTitle2);

  // Let the navigation commit and finish loading.
  response.Send(net::HTTP_OK, "text/html", "<title>Slow</title>");
  response.Done();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_TRUE(base::test::RunUntil([&] {
    return GetInfo().source == slow_url.spec() && GetInfo().title == "Slow" &&
           !GetInfo().is_loading;
  }));
}

}  // namespace
