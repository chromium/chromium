// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

constexpr base::TimeDelta kTestLatency = base::Milliseconds(100);

}  // namespace

class InitialWebUIWindowMetricsManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kInitialWebUIMetrics);
    EXPECT_CALL(browser_window_, GetProfile())
        .WillRepeatedly(testing::Return(&profile_));
    EXPECT_CALL(browser_window_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  MockBrowserWindowInterface browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(InitialWebUIWindowMetricsManagerTest,
       OnBrowserWindowFirstPresentationForNewWindow) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  manager.SetWindowCreationInfo(
      waap::NewWindowCreationSource::kBrowserInitiated, start_time);
  base::HistogramTester tester;
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "FirstPaint.FromConstructor",
      kTestLatency, 0);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.BrowserInitiated.WithoutExistingWindow."
      "BrowserWindow.FirstPaint.FromConstructor",
      kTestLatency, 0);

  manager.SkipStartupForTesting();

  base::TimeTicks timestamp = start_time + kTestLatency;
  manager.OnBrowserWindowFirstPresentation(timestamp);

  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "FirstPaint.FromConstructor",
      kTestLatency, 1);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.BrowserInitiated.WithoutExistingWindow."
      "BrowserWindow.FirstPaint.FromConstructor",
      kTestLatency, 1);
}

TEST_F(InitialWebUIWindowMetricsManagerTest, RecordsFirstPaintGapDelta) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  manager.SetWindowCreationInfo(
      waap::NewWindowCreationSource::kBrowserInitiated, start_time);
  manager.SkipStartupForTesting();

  base::HistogramTester tester;

  // Simulate presenting native window.
  base::TimeTicks browser_window_time = start_time + kTestLatency;
  manager.OnBrowserWindowFirstPresentation(browser_window_time);

  // Still no metric because WebUI hasn't painted.
  tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.BrowserWindowToReloadButton."
      "FirstPaintGap",
      0);

  // Simulate paint of WebUI reload button.
  base::TimeDelta webui_delay = base::Milliseconds(50);
  base::TimeTicks reload_button_time = browser_window_time + webui_delay;
  manager.OnReloadButtonFirstPaint(reload_button_time);

  // Now the gap metric should be emitted with the correct delta
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap",
      webui_delay, 1);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.BrowserInitiated.WithoutExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap",
      webui_delay, 1);
}

TEST_F(InitialWebUIWindowMetricsManagerTest, RecordsShowRequestedToFirstPaint) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  manager.SetWindowCreationInfo(
      waap::NewWindowCreationSource::kBrowserInitiated, start_time);
  manager.SkipStartupForTesting();

  base::HistogramTester tester;

  // Simulate show request.
  base::TimeDelta show_request_delay = base::Milliseconds(30);
  base::TimeTicks show_request_time = start_time + show_request_delay;
  manager.OnBrowserWindowShowRequested(show_request_time);

  // Still no metric because first presentation hasn't happened.
  tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "ShowRequestedToFirstPaint",
      0);

  // Simulate presenting native window.
  base::TimeDelta total_delay = base::Milliseconds(100);
  base::TimeTicks browser_window_time = start_time + total_delay;
  manager.OnBrowserWindowFirstPresentation(browser_window_time);

  // ShowRequestedToFirstPaint should be the time between show request and
  // presentation.
  base::TimeDelta expected_delta = total_delay - show_request_delay;

  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "ShowRequestedToFirstPaint.FromConstructor",
      expected_delta, 1);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.BrowserInitiated.WithoutExistingWindow."
      "BrowserWindow."
      "ShowRequestedToFirstPaint."
      "FromConstructor",
      expected_delta, 1);
}

TEST_F(InitialWebUIWindowMetricsManagerTest,
       ShowRequestedToFirstPaintIsIdempotent) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  manager.SetWindowCreationInfo(
      waap::NewWindowCreationSource::kBrowserInitiated, start_time);
  manager.SkipStartupForTesting();

  base::HistogramTester tester;

  // 1st show request.
  base::TimeDelta first_delay = base::Milliseconds(30);
  base::TimeTicks first_time = start_time + first_delay;
  manager.OnBrowserWindowShowRequested(first_time);

  // 2nd show request, which should be ignored.
  base::TimeDelta second_delay = base::Milliseconds(60);
  base::TimeTicks second_time = start_time + second_delay;
  manager.OnBrowserWindowShowRequested(second_time);

  // Simulate presentation.
  base::TimeDelta total_delay = base::Milliseconds(100);
  base::TimeTicks browser_window_time = start_time + total_delay;
  manager.OnBrowserWindowFirstPresentation(browser_window_time);

  // Baseline should be the 1st show request.
  base::TimeDelta expected_delta = total_delay - first_delay;

  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "ShowRequestedToFirstPaint."
      "FromConstructor",
      expected_delta, 1);
}

TEST_F(InitialWebUIWindowMetricsManagerTest,
       RecordsNewWindowIfStartupDeltaIsNegative) {
  // Ensure pristine static state across tests if tests are run in the same
  // process.
  InitialWebUIWindowMetricsManager::ResetForTesting();

  base::HistogramTester tester;

  {
    // Window 1: Startup Window (First window created)
    const base::TimeTicks start_time1 = base::TimeTicks::Now();
    InitialWebUIWindowMetricsManager startup_manager(&browser_window_);
    // We do NOT call `SkipStartupForTesting()` because we want to test the
    // startup logic.

    // Simulate presentation of native window.
    base::TimeTicks browser_window_time1 = start_time1 + kTestLatency;
    startup_manager.OnBrowserWindowFirstPresentation(browser_window_time1);

    // Simulate an invalid/negative gap (reload button paints BEFORE the window
    // presentation).
    base::TimeTicks reload_button_time1 =
        browser_window_time1 - base::Milliseconds(10);
    startup_manager.OnReloadButtonFirstPaint(reload_button_time1);
  }

  // Verify the startup metric was NOT recorded because the gap is negative.
  tester.ExpectTotalCount(
      "InitialWebUI.Startup.BrowserWindowToReloadButton.FirstPaintGap", 0);

  base::TimeDelta webui_delay = base::Milliseconds(50);
  {
    // Window 2: New Window
    const base::TimeTicks start_time2 = base::TimeTicks::Now();
    InitialWebUIWindowMetricsManager new_window_manager(&browser_window_);
    new_window_manager.SetWindowCreationInfo(
        waap::NewWindowCreationSource::kBrowserInitiated, start_time2);

    // Simulate presentation of native window.
    base::TimeTicks browser_window_time2 = start_time2 + kTestLatency;
    new_window_manager.OnBrowserWindowFirstPresentation(browser_window_time2);

    // Simulate a valid positive gap.
    base::TimeTicks reload_button_time2 = browser_window_time2 + webui_delay;
    new_window_manager.OnReloadButtonFirstPaint(reload_button_time2);
  }

  // The critical verification: This window must incorrectly NOT be logged as
  // Startup, but correctly as a New Window.
  tester.ExpectTotalCount(
      "InitialWebUI.Startup.BrowserWindowToReloadButton.FirstPaintGap", 0);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap",
      webui_delay, 1);
}

TEST_F(InitialWebUIWindowMetricsManagerTest,
       RecordsClosedBeforeFirstPaintForNewWindow) {
  InitialWebUIWindowMetricsManager::ResetForTesting();
  const base::TimeTicks start_time = base::TimeTicks::Now();
  base::HistogramTester tester;

  {
    InitialWebUIWindowMetricsManager manager(&browser_window_);
    manager.SetWindowCreationInfo(
        waap::NewWindowCreationSource::kBrowserInitiated, start_time);
    manager.SkipStartupForTesting();

    // Simulate show request.
    base::TimeDelta show_request_delay = base::Milliseconds(30);
    base::TimeTicks show_request_time = start_time + show_request_delay;
    manager.OnBrowserWindowShowRequested(show_request_time);

    // Destruction happens here when 'manager' goes out of scope.
  }

  // Verify metric was recorded.
  tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "ClosedBeforeFirstPaint",
      1);
  tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.BrowserInitiated.WithoutExistingWindow."
      "BrowserWindow.ClosedBeforeFirstPaint",
      1);
}

TEST_F(InitialWebUIWindowMetricsManagerTest,
       RecordsClosedBeforeFirstPaintForStartupWindow) {
  InitialWebUIWindowMetricsManager::ResetForTesting();
  const base::TimeTicks start_time = base::TimeTicks::Now();
  base::HistogramTester tester;

  {
    InitialWebUIWindowMetricsManager manager(&browser_window_);

    // Simulate show request.
    base::TimeDelta show_request_delay = base::Milliseconds(30);
    base::TimeTicks show_request_time = start_time + show_request_delay;
    manager.OnBrowserWindowShowRequested(show_request_time);

    // Destruction happens here when 'manager' goes out of scope.
  }

  // Verify metric was recorded.
  tester.ExpectTotalCount(
      "InitialWebUI.Startup.BrowserWindow.ClosedBeforeFirstPaint", 1);
}
