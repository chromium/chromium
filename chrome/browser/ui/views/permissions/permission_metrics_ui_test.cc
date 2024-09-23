// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

constexpr char kRequestNotifications[] = R"(
      new Promise(resolve => {
        Notification.requestPermission().then(function (permission) {
          resolve(permission)
        });
      })
      )";

class PermissionPromptMetricsTest : public InProcessBrowserTest {
 public:
  PermissionPromptMetricsTest() = default;
  PermissionPromptMetricsTest(const PermissionPromptMetricsTest&) = delete;
  PermissionPromptMetricsTest& operator=(const PermissionPromptMetricsTest&) =
      delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(PermissionPromptMetricsTest,
                       IgnoreReasonUmaTestCloseOtherTab) {
  base::HistogramTester histograms;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* embedder_contents_tab_0 =
      tab_strip->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents_tab_0);
  content::RenderFrameHost* rfh_tab_0 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);

  // Request Notification permission
  permissions::PermissionRequestObserver observer_tab_0(
      embedder_contents_tab_0);
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_0, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  observer_tab_0.Wait();

  // Open new tab
  chrome::NewTabToRight(browser());
  EXPECT_EQ(2, tab_strip->count());

  // Close old tab
  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip->GetWebContentsAt(0));
  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
      "IgnoredReason",
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionIgnoredReason::TAB_CLOSED),
      1);
}

IN_PROC_BROWSER_TEST_F(PermissionPromptMetricsTest,
                       IgnoreReasonUmaTestCloseCurrentTab) {
  base::HistogramTester histograms;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* embedder_contents_tab_0 =
      tab_strip->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents_tab_0);
  content::RenderFrameHost* rfh_tab_0 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);

  // Request Notification permission
  permissions::PermissionRequestObserver observer_tab_0(
      embedder_contents_tab_0);
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_0, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  observer_tab_0.Wait();

  // Open new tab
  chrome::NewTabToRight(browser());
  EXPECT_EQ(2, tab_strip->count());

  // Go to previous tab and close it
  tab_strip->ActivateTabAt(0);
  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip->GetWebContentsAt(0));
  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
      "IgnoredReason",
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionIgnoredReason::TAB_CLOSED),
      1);
}

IN_PROC_BROWSER_TEST_F(PermissionPromptMetricsTest,
                       IgnoreReasonUmaTestCloseBrowser) {
  base::HistogramTester histograms;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* embedder_contents_tab_0 =
      tab_strip->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents_tab_0);
  content::RenderFrameHost* rfh_tab_0 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  // Request Notification permission
  permissions::PermissionRequestObserver observer_tab_0(
      embedder_contents_tab_0);
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_0, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  observer_tab_0.Wait();

  // Close browser without decision
  chrome::CloseWindow(browser());
  ui_test_utils::WaitForBrowserToClose(browser());

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
      "IgnoredReason",
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionIgnoredReason::WINDOW_CLOSED),
      1);
}

IN_PROC_BROWSER_TEST_F(PermissionPromptMetricsTest,
                       IgnoreReasonUmaTestNavigation) {
  base::HistogramTester histograms;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* embedder_contents_tab_0 =
      tab_strip->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents_tab_0);
  content::RenderFrameHost* rfh_tab_0 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  // Request Notification permission
  permissions::PermissionRequestObserver observer_tab_0(
      embedder_contents_tab_0);
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_0, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  observer_tab_0.Wait();

  // Navigation
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
      "IgnoredReason",
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionIgnoredReason::NAVIGATION),
      1);
}
