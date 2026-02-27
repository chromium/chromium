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
      "InitialWebUI.NewWindow.AllSources.BrowserWindow.FirstPaint."
      "FromConstructor",
      kTestLatency, 0);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.BrowserInitiated.BrowserWindow.FirstPaint."
      "FromConstructor",
      kTestLatency, 0);

  manager.SkipStartupForTesting();

  base::TimeTicks timestamp = start_time + kTestLatency;
  manager.OnBrowserWindowFirstPresentation(timestamp);

  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.BrowserWindow.FirstPaint."
      "FromConstructor",
      kTestLatency, 1);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.BrowserInitiated.BrowserWindow.FirstPaint."
      "FromConstructor",
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
      "InitialWebUI.NewWindow.AllSources.BrowserWindowToReloadButton."
      "FirstPaintGap",
      webui_delay, 1);
  tester.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.BrowserInitiated.BrowserWindowToReloadButton."
      "FirstPaintGap",
      webui_delay, 1);
}
