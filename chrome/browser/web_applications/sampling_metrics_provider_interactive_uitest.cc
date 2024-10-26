// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/sampling_metrics_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace web_app {
namespace {

// This test class relies on the assumption that the only source of histogram
// emissions is the manual calls to SamplingMetricsProvider::EmitMetrics. This
// is currently guaranteed since the time between natural emissions (5 minutes)
// is significantly larger than test timeout.
class SamplingMetricsProviderInteractiveUiTest : public WebAppBrowserTestBase {
 public:
  void CheckWebAppCount(int web_app_count, bool is_active) {
    base::HistogramTester tester;
    web_app::SamplingMetricsProvider::EmitMetrics();

    // bucket0 means: web apps are not active
    // bucket1 means: web apps are active
    int bucket0_count = is_active ? 0 : 1;
    int bucket1_count = is_active ? 1 : 0;
    EXPECT_THAT(tester.GetAllSamples("WebApp.Engagement2.Active"),
                BucketsAre(base::Bucket(/*min=*/0, /*count=*/bucket0_count),
                           base::Bucket(/*min=*/1, /*count=*/bucket1_count)));
    EXPECT_THAT(tester.GetAllSamples("WebApp.Engagement2.Count"),
                BucketsAre(base::Bucket(/*min=*/web_app_count, /*count=*/1)));
  }
};

// TODO(https://crbug.com/358404364): The test works correctly but the
// production logic is broken on macOS.
#if !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest,
                       OpenCloseAppBrowser) {
  // There are no web-apps open by default.
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);

  // Install and launch an app browser.
  auto example_url = GURL("https://www.example.com");
  webapps::AppId app_id = InstallPWA(example_url);
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  CheckWebAppCount(/*web_app_count=*/1, /*is_active=*/true);

  // Close.
  chrome::CloseWindow(app_browser);
  ui_test_utils::WaitForBrowserToClose(app_browser);
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);
}
#endif  // !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest, Tab) {
  // There are no web-apps open by default.
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);

  // Install and launch a tabbed pwa.
  auto example_url = GURL("https://www.example.com");
  webapps::AppId app_id = InstallPWA(example_url);
  Browser* browser = LaunchBrowserForWebAppInTab(app_id);
  CheckWebAppCount(/*web_app_count=*/1, /*is_active=*/true);

  // Add a new tab that is not a PWA.
  AddBlankTabAndShow(browser);
  CheckWebAppCount(/*web_app_count=*/1, /*is_active=*/false);

  // Navigate the background PWA tab. Need to wait for navigation commit.
  // There are 3 tabs, so we want to navigate the second one.
  EXPECT_EQ(browser->tab_strip_model()->count(), 3);
  content::NavigationController::LoadURLParams params(GURL("about:blank"));
  auto* contents = browser->tab_strip_model()->GetTabAtIndex(1)->GetContents();
  CHECK(contents->GetController().LoadURLWithParams(params));
  content::WaitForLoadStop(contents);
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);
}

}  // namespace
}  // namespace web_app

