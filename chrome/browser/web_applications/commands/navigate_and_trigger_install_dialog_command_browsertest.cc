// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

class NavigateAndTriggerInstallDialogCommandTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  const GURL kOriginUrl = GURL("https://test.com");
  NavigateAndTriggerInstallDialogCommandTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kWebAppUniversalInstall);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kWebAppUniversalInstall);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(NavigateAndTriggerInstallDialogCommandTest,
                       OpensTheUrlInANewBrowserTab) {
  GURL test_url = GetInstallableAppURL();
  ASSERT_TRUE(test_url.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  // The browser should have one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->GetTabCount());

  content::TestNavigationObserver navigation_observer(test_url);
  navigation_observer.StartWatchingNewWebContents();

  base::RunLoop loop;
  provider().scheduler().ScheduleNavigateAndTriggerInstallDialog(
      test_url, kOriginUrl, /*is_renderer_initiated=*/true,
      base::BindLambdaForTesting(
          [&](NavigateAndTriggerInstallDialogCommandResult result) {
            loop.Quit();
          }));
  navigation_observer.Wait();
  // The browser should now have 2 tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->GetTabCount());
  // The active tab should be the |test_url| we navigated to.
  EXPECT_EQ(test_url,
            chrome_test_utils::GetActiveWebContents(this)->GetVisibleURL());

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(NavigateAndTriggerInstallDialogCommandTest,
                       TerminatesIfTabIsClosed) {
  GURL test_url = GetInstallableAppURL();
  ASSERT_TRUE(test_url.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  content::TestNavigationObserver navigation_observer(test_url);
  navigation_observer.StartWatchingNewWebContents();

  base::RunLoop loop;
  provider().scheduler().ScheduleNavigateAndTriggerInstallDialog(
      test_url, kOriginUrl, /*is_renderer_initiated=*/true,
      base::BindLambdaForTesting(
          [&](NavigateAndTriggerInstallDialogCommandResult result) {
            EXPECT_EQ(result,
                      NavigateAndTriggerInstallDialogCommandResult::kFailure);
            loop.Quit();
          }));
  navigation_observer.Wait();

  // Close the tab after we navigated to |test_url|.
  chrome_test_utils::GetActiveWebContents(this)->ClosePage();

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(NavigateAndTriggerInstallDialogCommandTest,
                       DoesNotTriggerDialogIfNotWebApp) {
  GURL test_url = https_server()->GetURL("/banners/no_manifest_test_page.html");
  ASSERT_TRUE(test_url.SchemeIs(url::kHttpsScheme));
  ASSERT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  provider().scheduler().ScheduleNavigateAndTriggerInstallDialog(
      test_url, kOriginUrl, /*is_renderer_initiated=*/true,
      base::BindLambdaForTesting(
          [&](NavigateAndTriggerInstallDialogCommandResult result) {
            EXPECT_EQ(result,
                      NavigateAndTriggerInstallDialogCommandResult::kFailure);
            loop.Quit();
          }));

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(NavigateAndTriggerInstallDialogCommandTest,
                       DoesNotTriggerDialogIfAlreadyInstalled) {
  GURL test_url = GetInstallableAppURL();
  ASSERT_TRUE(test_url.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  InstallPWA(test_url);

  base::RunLoop loop;
  provider().scheduler().ScheduleNavigateAndTriggerInstallDialog(
      test_url, kOriginUrl, /*is_renderer_initiated=*/true,
      base::BindLambdaForTesting(
          [&](NavigateAndTriggerInstallDialogCommandResult result) {
            EXPECT_EQ(result, NavigateAndTriggerInstallDialogCommandResult::
                                  kAlreadyInstalled);
            loop.Quit();
          }));

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(NavigateAndTriggerInstallDialogCommandTest,
                       CanTriggerWebAppDialog) {
  GURL test_url = GetInstallableAppURL();
  ASSERT_TRUE(test_url.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  provider().scheduler().ScheduleNavigateAndTriggerInstallDialog(
      test_url, kOriginUrl, /*is_renderer_initiated=*/true,
      base::BindLambdaForTesting(
          [&](NavigateAndTriggerInstallDialogCommandResult result) {
            EXPECT_EQ(
                result,
                NavigateAndTriggerInstallDialogCommandResult::kDialogShown);
            loop.Quit();
          }));

  loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         NavigateAndTriggerInstallDialogCommandTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WebAppSimpleInstallDialog"
                                             : "PWAConfirmationBubbleView";
                         });

}  // namespace web_app
