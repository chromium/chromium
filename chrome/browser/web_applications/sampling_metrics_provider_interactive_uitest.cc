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

// TODO(https://crbug.com/358404364): Make tests and/or production logic work on
// macOS.
#if !BUILDFLAG(IS_MAC)

namespace web_app {

// This test class relies on the assumption that the only source of histogram
// emissions is the manual calls to SamplingMetricsProvider::EmitMetrics. This
// is currently guaranteed since the time between natural emissions (5 minutes)
// is significantly larger than test timeout.
class SamplingMetricsProviderInteractiveUiTest : public WebAppBrowserTestBase {
 public:
  void CheckWebAppCount(base::HistogramTester* tester, int web_app_count) {
    // bucket0 means: web apps are not active
    // bucket1 means: web apps are active
    int bucket0_count = web_app_count >= 1 ? 0 : 1;
    int bucket1_count = web_app_count >= 1 ? 1 : 0;
    EXPECT_THAT(tester->GetAllSamples("WebApp.Engagement2.Active"),
                BucketsAre(base::Bucket(/*min=*/0, /*count=*/bucket0_count),
                           base::Bucket(/*min=*/1, /*count=*/bucket1_count)));
    EXPECT_THAT(tester->GetAllSamples("WebApp.Engagement2.Count"),
                BucketsAre(base::Bucket(/*min=*/web_app_count, /*count=*/1)));
  }
};

IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest,
                       OpenCloseAppBrowser) {
  // There are no web-apps open by default. Check that the stats are correct.
  {
    base::HistogramTester histogram_tester;
    web_app::SamplingMetricsProvider::EmitMetrics();
    CheckWebAppCount(&histogram_tester, 0);
  }

  // After installing and launching an app browser, checks that the stats are
  // correct.
  Browser* app_browser = nullptr;
  {
    base::HistogramTester histogram_tester;
    auto example_url = GURL("http://www.example.com");
    webapps::AppId app_id = InstallPWA(example_url);
    app_browser = LaunchWebAppBrowserAndWait(app_id);

    web_app::SamplingMetricsProvider::EmitMetrics();
    CheckWebAppCount(&histogram_tester, 1);
  }

  // Close and check stats are correct.
  {
    base::HistogramTester histogram_tester;
    chrome::CloseWindow(app_browser);
    ui_test_utils::WaitForBrowserToClose(app_browser);

    web_app::SamplingMetricsProvider::EmitMetrics();
    CheckWebAppCount(&histogram_tester, 0);
  }
}

}  // namespace web_app

#endif  // !BUILDFLAG(IS_MAC)
