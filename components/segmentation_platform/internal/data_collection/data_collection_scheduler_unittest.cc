// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/data_collection_scheduler.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class TestTrainingDataCollector : public TrainingDataCollector {
 public:
  TestTrainingDataCollector() = default;
  ~TestTrainingDataCollector() override = default;

  // TrainingDataCollector implementation.
  void OnModelMetadataUpdated() override{};
  void OnServiceInitialized() override{};
  void ReportCollectedContinuousTrainingData() override {
    training_data_reported_ = true;
  }

  bool training_data_reported() const { return training_data_reported_; }

 private:
  bool training_data_reported_ = false;
  base::OnceClosure quit_closure_;
};

class DataCollectionSchedulerTest : public ::testing::Test {
 public:
  DataCollectionSchedulerTest() = default;
  ~DataCollectionSchedulerTest() override = default;

  void SetUp() override {
    SegmentationPlatformService::RegisterLocalStatePrefs(prefs_.registry());
    data_collection_scheduler_ = std::make_unique<DataCollectionScheduler>(
        &training_data_collector_, &prefs_, &clock_);
  }

  void AdvanceTime(base::TimeDelta time_delta) {
    clock_.Advance(time_delta);
    task_environment_.FastForwardBy(time_delta);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<DataCollectionScheduler> data_collection_scheduler_;
  TestTrainingDataCollector training_data_collector_;
  TestingPrefServiceSimple prefs_;
  base::SimpleTestClock clock_;
};

// If there are no prior report, report will happen immediately if
// the current time is larger than base::Time() + base::Hourse(24).
TEST_F(DataCollectionSchedulerTest, NoPriorReport) {
  data_collection_scheduler_->ReportTrainingDataIfApplicable();
  EXPECT_FALSE(training_data_collector_.training_data_reported());

  AdvanceTime(base::Hours(10));
  EXPECT_FALSE(training_data_collector_.training_data_reported());

  AdvanceTime(base::Hours(20));

  // training_data_collector_.ExpectTrainingDataReported();
  EXPECT_TRUE(training_data_collector_.training_data_reported());
}

// Tests that after reporting, newly collected data will be correctly reported
// on next day.
TEST_F(DataCollectionSchedulerTest, ReportOnNextDay) {
  base::Time last_collection;
  EXPECT_TRUE(base::Time::FromString("Thu, 15 Oct 2021 20:45:00 GMT",
                                     &last_collection));
  prefs_.SetTime(kSegmentationLastCollectionTimePref, last_collection);
  clock_.SetNow(last_collection);
  data_collection_scheduler_->ReportTrainingDataIfApplicable();

  // Within the same day, no report will happen.
  AdvanceTime(base::Hours(3));
  EXPECT_FALSE(training_data_collector_.training_data_reported());

  // No reporting if last reporting is within time limit.
  AdvanceTime(base::Hours(5));
  EXPECT_FALSE(training_data_collector_.training_data_reported());

  AdvanceTime(base::Hours(5));
  EXPECT_TRUE(training_data_collector_.training_data_reported());
}

}  // namespace segmentation_platform