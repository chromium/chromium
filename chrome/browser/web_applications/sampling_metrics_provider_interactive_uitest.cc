// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/sampling_metrics_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace web_app {
namespace {

// This test class relies on the assumption that the only source of histogram
// emissions is the manual calls to SamplingMetricsProvider::EmitMetrics. This
// is currently guaranteed since the time between natural emissions (5 minutes)
// is significantly larger than test timeout.
class SamplingMetricsProviderInteractiveUiTest : public WebAppBrowserTestBase {
 public:
  void CheckWebAppCount(int web_app_count, bool is_active) {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
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

    if (web_app_count == 0) {
      return;
    }

    // Flush UKM.
    using UkmEntry = ukm::builders::WebApp_DailyInteraction;
    FlushUkm(ukm_recorder);

    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    auto* entry = entries[0].get();
    ukm_recorder.ExpectEntrySourceHasUrl(entry, GetInstallableAppURL());
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry, UkmEntry::kUsedName,
                                                   true);
    if (is_active) {
      EXPECT_TRUE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
          entry, UkmEntry::kForegroundDurationName));
      EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
          entry, UkmEntry::kBackgroundDurationName));
    } else {
      EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
          entry, UkmEntry::kForegroundDurationName));
      EXPECT_TRUE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
          entry, UkmEntry::kBackgroundDurationName));
    }
  }

  void FlushUkm(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
    using UkmEntry = ukm::builders::WebApp_DailyInteraction;
    FlushAllRecordsForTesting(profile());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return ukm_recorder.GetEntriesByName(UkmEntry::kEntryName).size() == 1u;
    }));
  }

  void CheckOneUkmEntry() {
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    // The UKM emission uses a bucket size of 28.8 minutes. We want to
    // distinguish between double and single emission. Each time we emit metrics
    // we emit 5 minutes of total time. This means we want to call this method 6
    // times. We expect to end up in bucket 2 (without duplicate emissions).
    // With duplicate emissions we'd end up in bucket 4.
    for (int i = 0; i < 6; ++i) {
      web_app::SamplingMetricsProvider::EmitMetrics();
    }
    FlushUkm(ukm_recorder);
    using UkmEntry = ukm::builders::WebApp_DailyInteraction;

    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    auto* entry = entries[0].get();

    // The total foreground/background time should be equal to 30 minutes, which
    // is bucket 2.
    int64_t total_time = 0;
    const int64_t* foreground_time =
        ukm::TestAutoSetUkmRecorder::GetEntryMetric(
            entry, UkmEntry::kForegroundDurationName);
    if (foreground_time) {
      total_time += *foreground_time;
    }
    const int64_t* background_time =
        ukm::TestAutoSetUkmRecorder::GetEntryMetric(
            entry, UkmEntry::kBackgroundDurationName);
    if (background_time) {
      total_time += *background_time;
    }
    int expected_result = 24 * 60 * 60 / 50;
    EXPECT_EQ(total_time, expected_result);
  }

  webapps::AppId InstallTabbedPWA(const GURL& start_url) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
    web_app_info->title = u"A Web App";
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }
};

IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest,
                       OpenCloseAppBrowser) {
  // There are no web-apps open by default.
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);

  // Install and launch an app browser.
  webapps::AppId app_id = InstallPWA(GetInstallableAppURL());
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  CheckWebAppCount(/*web_app_count=*/1, /*is_active=*/true);

  // Close.
  chrome::CloseWindow(app_browser);
  ui_test_utils::WaitForBrowserToClose(app_browser);
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);
}

IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest, Tab) {
  // There are no web-apps open by default.
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);

  // Install and launch a tabbed pwa.
  webapps::AppId app_id = InstallTabbedPWA(GetInstallableAppURL());
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

// If the same PWA as multiple tabs open, only a single UKM event should be
// emitted.
IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest,
                       MultipleTabsSingleUkm) {
  webapps::AppId app_id = InstallTabbedPWA(GetInstallableAppURL());
  LaunchBrowserForWebAppInTab(app_id);
  LaunchBrowserForWebAppInTab(app_id);
  CheckOneUkmEntry();
}

// If the same PWA as multiple apps open, only a single UKM event should be
// emitted.
IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest,
                       MultipleAppsSingleUkm) {
  webapps::AppId app_id = InstallPWA(GetInstallableAppURL());
  LaunchWebAppBrowserAndWait(app_id);
  LaunchWebAppBrowserAndWait(app_id);
  CheckOneUkmEntry();
}

// Basic case for promotable.
IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest, Promotable) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL());
  AddBlankTabAndShow(browser());
  NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL());

  web_app::SamplingMetricsProvider::EmitMetrics();
  FlushUkm(ukm_recorder);
  using UkmEntry = ukm::builders::WebApp_DailyInteraction;

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  auto* entry = entries[0].get();
  const int64_t* promotable = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
      entry, UkmEntry::kPromotableName);
  ASSERT_TRUE(promotable);
  ASSERT_TRUE(*promotable);
}

// If PWA is installed as standalone, opening a tab does not count towards UKMs.
IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest,
                       MismatchedDisplayMode) {
  webapps::AppId app_id = InstallPWA(GetInstallableAppURL());
  LaunchBrowserForWebAppInTab(app_id);

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Remove asynchronous code from FlushAllRecordsForTesting.
  SkipOriginCheckForTesting();

  // This should not emit any metrics.
  web_app::SamplingMetricsProvider::EmitMetrics();
  FlushAllRecordsForTesting(profile());

  // There should be no emissions.
  using UkmEntry = ukm::builders::WebApp_DailyInteraction;
  ASSERT_EQ(ukm_recorder.GetEntriesByName(UkmEntry::kEntryName).size(), 0u);
}

// Incognito windows should not cause crashes.
IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderInteractiveUiTest,
                       IncognitoWindow) {
  CreateIncognitoBrowser(profile());
  CheckWebAppCount(/*web_app_count=*/0, /*is_active=*/false);
}

}  // namespace
}  // namespace web_app

