// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class PerformanceControlsMetricsTest
    : public content::RenderViewHostTestHarness {
 public:
  PerformanceControlsMetricsTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    PerformanceInterventionMetricsReporter::RegisterLocalStatePrefs(
        prefs()->registry());
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

TEST_F(PerformanceControlsMetricsTest, DailyMetricsResets) {
  base::HistogramTester histogram_tester;

  const std::string message_count_histogram_name =
      "PerformanceControls.Intervention.BackgroundTab.Cpu.MessageShownCount";
  const std::string rate_limited_histogram_name =
      "PerformanceControls.Intervention.BackgroundTab.Cpu.RateLimitedCount";

  PrefService* const pref_service = prefs();
  std::unique_ptr<PerformanceInterventionMetricsReporter> daily_metrics =
      std::make_unique<PerformanceInterventionMetricsReporter>(pref_service);

  EXPECT_EQ(0, pref_service->GetInteger(
                   prefs::kPerformanceInterventionBackgroundCpuMessageCount));
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 1, 0);
  histogram_tester.ExpectBucketCount(rate_limited_histogram_name, 1, 0);

  RecordInterventionMessageCount(
      performance_manager::user_tuning::PerformanceDetectionManager::
          ResourceType::kCpu,
      pref_service);

  // Only the pref count should increment when we record a message count since
  // the histogram should record the value only after each day.
  EXPECT_EQ(1, pref_service->GetInteger(
                   prefs::kPerformanceInterventionBackgroundCpuMessageCount));
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 1, 0);

  RecordInterventionRateLimitedCount(
      performance_manager::user_tuning::PerformanceDetectionManager::
          ResourceType::kCpu,
      pref_service);
  EXPECT_EQ(1,
            pref_service->GetInteger(
                prefs::kPerformanceInterventionBackgroundCpuRateLimitedCount));
  histogram_tester.ExpectBucketCount(rate_limited_histogram_name, 1, 0);

  // Message and rate limit count should be recorded to the histogram
  // and the prefs should be reset after a day
  task_environment()->FastForwardBy(base::Days(1));
  EXPECT_EQ(0, pref_service->GetInteger(
                   prefs::kPerformanceInterventionBackgroundCpuMessageCount));
  EXPECT_EQ(0,
            pref_service->GetInteger(
                prefs::kPerformanceInterventionBackgroundCpuRateLimitedCount));
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 1, 1);
  histogram_tester.ExpectBucketCount(rate_limited_histogram_name, 1, 1);
}
