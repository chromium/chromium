// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_mock_clock_override.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace web_app {

using UkmEntry = ukm::builders::WebApp_DailyInteraction;
using testing::Contains;
using testing::Key;
using testing::Not;
using testing::Pair;

// Tests for web app metrics recording.
// Note that there are further tests of the daily metrics emitting behavior in
// |DailyMetricsHelperTest|.
class WebAppMetricsBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppMetricsBrowserTest() = default;
  WebAppMetricsBrowserTest(const WebAppMetricsBrowserTest&) = delete;
  WebAppMetricsBrowserTest& operator=(const WebAppMetricsBrowserTest&) = delete;
  ~WebAppMetricsBrowserTest() override = default;

  void SetUp() override { WebAppControllerBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
    // Ignore real window activation which causes flakiness in tests.
    WebAppMetrics::Get(profile())->RemoveBrowserListObserverForTesting();
  }

  AppId InstallWebApp() {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = GetInstallableAppURL();
    web_app_info->title = u"A Web App";
    web_app_info->display_mode = DisplayMode::kStandalone;
    web_app_info->user_display_mode = UserDisplayMode::kStandalone;
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  GURL GetNonWebAppUrl() {
    return https_server()->GetURL("/banners/no_manifest_test_page.html");
  }

  void ForceEmitMetricsNow() {
    FlushAllRecordsForTesting(profile());
    // Ensure async call for origin check in daily_metrics_helper completes.
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }
};

IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       NonInstalledWebApp_RecordsDailyInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  AddBlankTabAndShow(browser());
  NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL());

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, GetInstallableAppURL());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry, UkmEntry::kUsedName,
                                                 true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kInstalledName, false);
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kInstallSourceName));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kDisplayModeName,
      static_cast<int>(DisplayMode::kStandalone));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kPromotableName, true);
  // Not installed, so should not have any session count or time.
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kForegroundDurationName));
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kBackgroundDurationName));
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kNumSessionsName));
}

IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       InstalledWebAppInTab_RecordsDailyInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = GetInstallableAppURL();
  web_app_info->title = u"A Web App";
  web_app_info->display_mode = DisplayMode::kStandalone;
  web_app_info->user_display_mode = UserDisplayMode::kStandalone;
  web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  AddBlankTabAndShow(browser());
  NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL());

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, GetInstallableAppURL());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry, UkmEntry::kUsedName,
                                                 true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kInstalledName, true);
  // |InstallWebApp| always sets |OMNIBOX_INSTALL_ICON| as the install source.
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kInstallSourceName,
      static_cast<int>(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kDisplayModeName,
      static_cast<int>(DisplayMode::kStandalone));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kPromotableName, true);
  // Not in window, so should not have any session count or time.
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kForegroundDurationName));
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kBackgroundDurationName));
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kNumSessionsName));
}

IN_PROC_BROWSER_TEST_F(
    WebAppMetricsBrowserTest,
    InstalledWebAppInWindow_RecordsDailyInteractionWithSessionDurations) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = GetInstallableAppURL();
  web_app_info->title = u"A Web App";
  web_app_info->display_mode = DisplayMode::kStandalone;
  web_app_info->user_display_mode = UserDisplayMode::kStandalone;
  AppId app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  LaunchWebAppBrowserAndAwaitInstallabilityCheck(app_id);

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, GetInstallableAppURL());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry, UkmEntry::kUsedName,
                                                 true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kInstalledName, true);
  // |InstallWebApp| always sets |OMNIBOX_INSTALL_ICON| as the install source.
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kInstallSourceName,
      static_cast<int>(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kDisplayModeName,
      static_cast<int>(DisplayMode::kStandalone));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kPromotableName, true);
  // No further interactions after navigating to the page, so no counted session
  // time is ok, but should count 1 session.
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kForegroundDurationName));
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kBackgroundDurationName));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry,
                                                 UkmEntry::kNumSessionsName, 1);
}

// Flaky test: crbug.com/1170786
IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       DISABLED_NonWebApp_RecordsNothing) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  NavigateAndAwaitInstallabilityCheck(browser(), GetNonWebAppUrl());

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 0U);
}

IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       NavigationsWithinInstalledWebApp_RecordsOneSession) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  AppId app_id = InstallWebApp();
  Browser* browser = LaunchWebAppBrowserAndAwaitInstallabilityCheck(app_id);
  NavigateAndAwaitInstallabilityCheck(browser, GetInstallableAppURL());
  NavigateAndAwaitInstallabilityCheck(browser, GetInstallableAppURL());
  NavigateAndAwaitInstallabilityCheck(browser, GetInstallableAppURL());
  NavigateAndAwaitInstallabilityCheck(browser, GetInstallableAppURL());

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto metrics = entries[0]->metrics;
  EXPECT_THAT(metrics, Contains(Pair(UkmEntry::kNumSessionsNameHash, 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       InstalledWebApp_RecordsTimeAndSessions) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AppId app_id = InstallWebApp();
  Browser* app_browser;

  // Open the app.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(1);
        },
        nullptr, nullptr);
    app_browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    DCHECK(app_browser);
    // Manually activate the web app window (observer is disabled for testing).
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
  }
  // Switch to a non-app window after 2 hours of foreground time.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(3);
        },
        nullptr, nullptr);
    WebAppMetrics::Get(profile())->OnBrowserNoLongerActive(app_browser);
  }
  // Switch back to the web app after 4 hours of background time.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(7);
        },
        nullptr, nullptr);
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
  }

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto metrics = entries[0]->metrics;
  // 2 hours = 7200 seconds. Nearest 1/50 day bucket is 6912.
  EXPECT_THAT(metrics,
              Contains(Pair(UkmEntry::kForegroundDurationNameHash, 6912)));
  // 4 hours = 14400 seconds. Nearest 1/50 day bucket is 13824.
  EXPECT_THAT(metrics,
              Contains(Pair(UkmEntry::kBackgroundDurationNameHash, 13824)));
  EXPECT_THAT(metrics, Contains(Pair(UkmEntry::kNumSessionsNameHash, 2)));
}

IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       InstalledWebApp_RecordsTimeAndSessionWhenClosed) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AppId app_id = InstallWebApp();
  Browser* app_browser;

  // Open the app.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(1);
        },
        nullptr, nullptr);
    app_browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    DCHECK(app_browser);
    // Manually activate the web app window (observer is disabled for testing).
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
  }

  // Close the app.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(3);
        },
        nullptr, nullptr);
    CloseBrowserSynchronously(app_browser);
  }

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto metrics = entries[0]->metrics;
  // 2 hours = 7200 seconds. Nearest 1/50 day bucket is 6912.
  EXPECT_THAT(metrics,
              Contains(Pair(UkmEntry::kForegroundDurationNameHash, 6912)));
  EXPECT_THAT(metrics, Contains(Pair(UkmEntry::kNumSessionsNameHash, 1)));
}

// Verify that the behavior with multiple web app instances is as expected, even
// though that behavior isn't completely accurate in recording time
// (crbug.com/1081187).
IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       MultipleWebAppInstances_StillRecordsTime) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AppId app_id = InstallWebApp();
  Browser* app_browser;

  // Open the app.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(1);
        },
        nullptr, nullptr);
    app_browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    DCHECK(app_browser);
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
    // Launch another app instance, emulating normal window activity.
    WebAppMetrics::Get(profile())->OnBrowserNoLongerActive(app_browser);
    Browser* browser_2 = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(browser_2);
    // Close the second instance without reactivating the first instance.
    WebAppMetrics::Get(profile())->OnBrowserNoLongerActive(browser_2);
    browser_2->tab_strip_model()->CloseAllTabs();
  }
  // Switch to the app after 2 hours of missed background time.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(3);
        },
        nullptr, nullptr);
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
  }
  // Switch away from the app after 4 hours of foreground time.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(7);
        },
        nullptr, nullptr);
    WebAppMetrics::Get(profile())->OnBrowserNoLongerActive(app_browser);
  }

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto metrics = entries[0]->metrics;
  // 4 hours = 14400 seconds. Nearest 1/50 day bucket is 13824.
  EXPECT_THAT(metrics,
              Contains(Pair(UkmEntry::kForegroundDurationNameHash, 13824)));
  // Background time is missed (crbug.com/1081187).
  EXPECT_THAT(metrics,
              Not(Contains(Key(UkmEntry::kBackgroundDurationNameHash))));
}

IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest,
                       InstalledWebApp_RecordsZeroTimeIfOverLimit) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AppId app_id = InstallWebApp();
  Browser* app_browser;

  // Open the app.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(1);
        },
        nullptr, nullptr);
    app_browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    DCHECK(app_browser);
    // Manually activate the web app window (observer is disabled for testing).
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
  }
  // Switch to a non-app window after 18 hours of foreground time (over limit).
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(18);
        },
        nullptr, nullptr);
    WebAppMetrics::Get(profile())->OnBrowserNoLongerActive(app_browser);
  }

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto metrics = entries[0]->metrics;
  EXPECT_THAT(metrics,
              Not(Contains(Key(UkmEntry::kForegroundDurationNameHash))));
  EXPECT_THAT(metrics,
              Not(Contains(Key(UkmEntry::kBackgroundDurationNameHash))));
  EXPECT_THAT(metrics, Contains(Pair(UkmEntry::kNumSessionsNameHash, 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppMetricsBrowserTest, Suspend_FlushesSessionTimes) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AppId app_id = InstallWebApp();
  Browser* app_browser;

  // Open the app.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(1);
        },
        nullptr, nullptr);
    app_browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    DCHECK(app_browser);
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
  }
  // Suspend after 2 hours of foreground time.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(3);
        },
        nullptr, nullptr);
    WebAppMetrics::Get(profile())->OnSuspend();
  }
  // Switch back to the web app after 4 hours of suspend time.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 base::Hours(7);
        },
        nullptr, nullptr);
    WebAppMetrics::Get(profile())->OnBrowserSetLastActive(app_browser);
  }

  ForceEmitMetricsNow();

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto* entry = entries[0];
  // 2 hours = 7200 seconds. Nearest 1/50 day bucket is 6912.
  EXPECT_THAT(entry->metrics,
              Contains(Pair(UkmEntry::kForegroundDurationNameHash, 6912)));
  // Suspend should clear timers so no background time is recorded.
  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, UkmEntry::kBackgroundDurationName));
}

}  // namespace web_app
