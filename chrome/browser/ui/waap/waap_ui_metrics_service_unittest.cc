// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class WaapUIMetricsServiceTest : public testing::Test {
 public:
  WaapUIMetricsServiceTest() = default;
  ~WaapUIMetricsServiceTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    // WaapUIMetricsService is only created when kInitialWebUI is enabled.
    feature_list_.InitAndEnableFeature(features::kInitialWebUI);
  }

 protected:
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  TestingProfile profile_;
};

// Tests that the WaapUIMetricsService is not created when the kInitialWebUI
// feature is disabled.
TEST(WaapUIMetricsServiceFeatureDisabledTest, ServiceNotCreated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInitialWebUI);

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  WaapUIMetricsService* service =
      WaapUIMetricsServiceFactory::GetForProfile(&profile);
  EXPECT_FALSE(service);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Tests that the OnFirstPaint method records a histogram on the first call,
// and does not record it again on subsequent calls.
TEST_F(WaapUIMetricsServiceTest, OnFirstPaint) {
  WaapUIMetricsService* service =
      WaapUIMetricsServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  const base::TimeTicks paint_time = base::TimeTicks::Now();
  service->OnFirstPaint(paint_time);

  // For tests, startup temperature is undetermined.
  histogram_tester()->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);

  // Call a second time to ensure it's not recorded again.
  service->OnFirstPaint(paint_time);
  histogram_tester()->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);
}

// Tests that the OnFirstContentfulPaint method records a histogram on the
// first call, and does not record it again on subsequent calls.
TEST_F(WaapUIMetricsServiceTest, OnFirstContentfulPaint) {
  WaapUIMetricsService* service =
      WaapUIMetricsServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  const base::TimeTicks paint_time = base::TimeTicks::Now();
  service->OnFirstContentfulPaint(paint_time);

  // For tests, startup temperature is undetermined.
  histogram_tester()->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);

  // Call a second time to ensure it's not recorded again.
  service->OnFirstContentfulPaint(paint_time);
  histogram_tester()->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
