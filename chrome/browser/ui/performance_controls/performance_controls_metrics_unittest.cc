// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using performance_manager::user_tuning::PerformanceDetectionManager;
}

class PerformanceControlsMetricsTest
    : public content::RenderViewHostTestHarness {
 public:
  PerformanceControlsMetricsTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  TestingPrefServiceSimple* prefs() {
    return scoped_testing_local_state_.Get();
  }

 private:
  ScopedTestingLocalState scoped_testing_local_state_{
      TestingBrowserProcess::GetGlobal()};
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
      PerformanceDetectionManager::ResourceType::kCpu, pref_service);

  // Only the pref count should increment when we record a message count since
  // the histogram should record the value only after each day.
  EXPECT_EQ(1, pref_service->GetInteger(
                   prefs::kPerformanceInterventionBackgroundCpuMessageCount));
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 1, 0);

  RecordInterventionRateLimitedCount(
      PerformanceDetectionManager::ResourceType::kCpu, pref_service);
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

TEST_F(PerformanceControlsMetricsTest, HealthStatusChange) {
  base::HistogramTester histogram_tester;
  const std::string message_count_histogram_name =
      "PerformanceControls.Intervention.BackgroundTab.Cpu.HealthStatusChange."
      "1Min";

  // Recording a health status that didn't change.
  RecordCpuHealthStatusChange(
      base::Minutes(1), PerformanceDetectionManager::HealthLevel::kHealthy,
      PerformanceDetectionManager::HealthLevel::kHealthy);
  histogram_tester.ExpectBucketCount(
      message_count_histogram_name,
      CpuInterventionHealthChange::kHealthyToHealthy, 1);

  // Recording a health status that improved by one level.
  RecordCpuHealthStatusChange(
      base::Minutes(1), PerformanceDetectionManager::HealthLevel::kUnhealthy,
      PerformanceDetectionManager::HealthLevel::kDegraded);
  histogram_tester.ExpectBucketCount(
      message_count_histogram_name,
      CpuInterventionHealthChange::kUnhealthyToDegraded, 1);

  // Recording a health status that improved by 2 levels.
  RecordCpuHealthStatusChange(
      base::Minutes(1), PerformanceDetectionManager::HealthLevel::kUnhealthy,
      PerformanceDetectionManager::HealthLevel::kHealthy);
  histogram_tester.ExpectBucketCount(
      message_count_histogram_name,
      CpuInterventionHealthChange::kUnhealthyToHealthy, 1);

  // Recording a health status that got worse.
  RecordCpuHealthStatusChange(
      base::Minutes(1), PerformanceDetectionManager::HealthLevel::kHealthy,
      PerformanceDetectionManager::HealthLevel::kUnhealthy);
  histogram_tester.ExpectBucketCount(
      message_count_histogram_name,
      CpuInterventionHealthChange::kHealthyToUnhealthy, 1);
}

class PerformanceControlsNotificationTest
    : public PerformanceControlsMetricsTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        performance_manager::features::
            kPerformanceInterventionNotificationImprovements);

    PerformanceControlsMetricsTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PerformanceControlsNotificationTest,
       RecordsNotificationAcceptPercentage) {
  base::HistogramTester histogram_tester;
  const std::string message_count_histogram_name =
      "PerformanceControls.Intervention.DailyAcceptancePercentage";
  std::unique_ptr<PerformanceInterventionButtonController> controller =
      std::make_unique<PerformanceInterventionButtonController>(nullptr,
                                                                nullptr);

  PrefService* const pref_service = prefs();
  std::unique_ptr<PerformanceInterventionMetricsReporter> daily_metrics =
      std::make_unique<PerformanceInterventionMetricsReporter>(pref_service);

  ASSERT_TRUE(
      pref_service
          ->GetList(performance_manager::user_tuning::prefs::
                        kPerformanceInterventionNotificationAcceptHistory)
          .empty());

  task_environment()->FastForwardBy(base::Days(1));
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 100, 0);

  pref_service->SetList(performance_manager::user_tuning::prefs::
                            kPerformanceInterventionNotificationAcceptHistory,
                        base::Value::List().Append(true));
  task_environment()->FastForwardBy(base::Days(1));
  EXPECT_EQ(100,
            PerformanceInterventionButtonController::GetAcceptancePercentage());
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 100, 1);

  base::Value::List updated_accept_history = base::Value::List();
  for (int i = 0;
       i < performance_manager::features::kAcceptanceRateWindowSize.Get() / 2;
       i++) {
    updated_accept_history.Append(true);
    updated_accept_history.Append(false);
  }
  pref_service->SetList(performance_manager::user_tuning::prefs::
                            kPerformanceInterventionNotificationAcceptHistory,
                        std::move(updated_accept_history));
  task_environment()->FastForwardBy(base::Days(1));
  EXPECT_EQ(50,
            PerformanceInterventionButtonController::GetAcceptancePercentage());
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 50, 1);
  histogram_tester.ExpectBucketCount(message_count_histogram_name, 100, 1);
}
