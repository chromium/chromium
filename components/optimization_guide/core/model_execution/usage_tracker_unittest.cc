// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/usage_tracker.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using Priority = UsageTracker::Priority;

class MockUsageTrackerObserver : public UsageTracker::Observer {
 public:
  MOCK_METHOD(void,
              OnPriorityIncrease,
              (const std::string& use_case_name, Priority previous_priority),
              (override));
};

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest() {
    model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
    usage_tracker_ = std::make_unique<UsageTracker>(&local_state_);
  }

  TestingPrefServiceSimple& local_state() { return local_state_; }
  UsageTracker& usage_tracker() { return *usage_tracker_; }

  void ResetUsageTracker() {
    usage_tracker_ = std::make_unique<UsageTracker>(&local_state_);
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<UsageTracker> usage_tracker_;
};

TEST_F(UsageTrackerTest, GetPriorityUserBlocking) {
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), Priority::kRetain);

  usage_tracker().RaisePriority("test_use_case", Priority::kUserBlocking);

  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kUserBlocking);
}

TEST_F(UsageTrackerTest, GetPriorityBestEffort) {
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), Priority::kRetain);

  usage_tracker().RaisePriority("test_use_case", Priority::kBestEffort);

  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kBestEffort);
}

TEST_F(UsageTrackerTest, RaisePriorityDoesNotLowerPriority) {
  usage_tracker().RaisePriority("test_use_case", Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kUserBlocking);

  usage_tracker().RaisePriority("test_use_case", Priority::kBestEffort);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kUserBlocking);
}

TEST_F(UsageTrackerTest, PersistenceAcrossRestart) {
  usage_tracker().RaisePriority("user_blocking_use_case",
                                Priority::kUserBlocking);
  usage_tracker().RaisePriority("best_effort_use_case", Priority::kBestEffort);

  EXPECT_EQ(usage_tracker().GetPriority("user_blocking_use_case"),
            Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("best_effort_use_case"),
            Priority::kBestEffort);

  // Simulate browser restart by re-instantiating UsageTracker with the same PrefService.
  ResetUsageTracker();

  // In-memory user blocking state is cleared. Only best effort priority is persisted in prefs.
  EXPECT_EQ(usage_tracker().GetPriority("user_blocking_use_case"),
            Priority::kBestEffort);
  EXPECT_EQ(usage_tracker().GetPriority("best_effort_use_case"),
            Priority::kBestEffort);

  // Raising priority back to user blocking upgrades it.
  usage_tracker().RaisePriority("user_blocking_use_case",
                                Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("user_blocking_use_case"),
            Priority::kUserBlocking);
}

TEST_F(UsageTrackerTest, SetPriority) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kOnDeviceModelEviction);

  usage_tracker().SetPriority("test_use_case", Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kUserBlocking);

  usage_tracker().SetPriority("test_use_case", Priority::kEvictable);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), Priority::kEvictable);
}

TEST_F(UsageTrackerTest, SetPriorityBestEffort) {
  usage_tracker().SetPriority("test_use_case", Priority::kBestEffort);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kBestEffort);
}

TEST_F(UsageTrackerTest, ObserverNotified) {
  MockUsageTrackerObserver observer;
  usage_tracker().AddObserver(&observer);

  EXPECT_CALL(observer, OnPriorityIncrease("test_use_case", Priority::kRetain))
      .Times(1);
  usage_tracker().RaisePriority("test_use_case", Priority::kBestEffort);

  EXPECT_CALL(observer,
              OnPriorityIncrease("test_use_case", Priority::kBestEffort))
      .Times(1);
  usage_tracker().RaisePriority("test_use_case", Priority::kUserBlocking);

  // Calling RaisePriority again with same or lower priority does not fire observer.
  EXPECT_CALL(observer, OnPriorityIncrease).Times(0);
  usage_tracker().RaisePriority("test_use_case", Priority::kUserBlocking);
  usage_tracker().RaisePriority("test_use_case", Priority::kBestEffort);

  usage_tracker().RemoveObserver(&observer);
}

TEST_F(UsageTrackerTest, ClearAllUseCaseUsages) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kOnDeviceModelEviction);

  usage_tracker().RaisePriority("use_case_1", Priority::kUserBlocking);
  usage_tracker().RaisePriority("use_case_2", Priority::kBestEffort);

  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"),
            Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"),
            Priority::kBestEffort);

  usage_tracker().ClearAllUseCaseUsages();

  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kEvictable);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"), Priority::kEvictable);
}

// TODO(crbug.com/548711885): Remove this test when scam detection code is
// updated to pass the right priority.
TEST_F(UsageTrackerTest, ScamDetectionNeverUserBlocking) {
  const std::string scam_detection_use_case =
      ToUseCaseName(mojom::OnDeviceFeature::kScamDetection);

  usage_tracker().RaisePriority(scam_detection_use_case,
                                Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority(scam_detection_use_case),
            Priority::kBestEffort);
}

// TODO(crbug.com/548711885): Remove this test when scam detection code is
// updated to pass the right priority.
TEST_F(UsageTrackerTest, ScamDetectionObserverNotifiedWithBestEffort) {
  const std::string scam_detection_use_case =
      ToUseCaseName(mojom::OnDeviceFeature::kScamDetection);
  MockUsageTrackerObserver observer;
  usage_tracker().AddObserver(&observer);

  EXPECT_CALL(observer,
              OnPriorityIncrease(scam_detection_use_case, Priority::kRetain))
      .Times(1);
  usage_tracker().RaisePriority(scam_detection_use_case,
                                Priority::kUserBlocking);

  // Calling RaisePriority again with kUserBlocking does not fire observer again
  // because priority was clamped to kBestEffort.
  EXPECT_CALL(observer, OnPriorityIncrease).Times(0);
  usage_tracker().RaisePriority(scam_detection_use_case,
                                Priority::kUserBlocking);

  usage_tracker().RemoveObserver(&observer);
}

TEST_F(UsageTrackerTest, RetentionPeriodPriorityRetain) {
  usage_tracker().RaisePriority("use_case_1", Priority::kUserBlocking);
  usage_tracker().RaisePriority("use_case_2", Priority::kBestEffort);

  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"), Priority::kBestEffort);

  // After 31 days (>30 days recent use window, <=90 days retention period),
  // priorities transition to Priority::kRetain.
  task_environment().FastForwardBy(base::Days(31));
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kRetain);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"), Priority::kRetain);

  // Across restart at 31 days, prefs are retained and priority remains kRetain.
  ResetUsageTracker();
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kRetain);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"), Priority::kRetain);

  // Raising priority from kRetain to kBestEffort notifies observer with
  // previous_priority = kRetain and does not restore stale kUserBlocking.
  MockUsageTrackerObserver observer;
  usage_tracker().AddObserver(&observer);
  EXPECT_CALL(observer, OnPriorityIncrease("use_case_1", Priority::kRetain))
      .Times(1);
  usage_tracker().RaisePriority("use_case_1", Priority::kBestEffort);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kBestEffort);
  usage_tracker().RemoveObserver(&observer);
}

TEST_F(UsageTrackerTest, PrefPruningAfterRetentionPeriod) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kOnDeviceModelEviction);

  usage_tracker().RaisePriority("use_case_1", Priority::kBestEffort);
  task_environment().FastForwardBy(base::Days(31));
  usage_tracker().RaisePriority("use_case_2", Priority::kBestEffort);

  // Advance another 60 days: use_case_1 is 91 days old (>90 days),
  // use_case_2 is 60 days old (30 < age <= 90 days).
  task_environment().FastForwardBy(base::Days(60));
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kEvictable);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"), Priority::kRetain);

  // On restart, UsageTracker prunes prefs older than 90 days.
  ResetUsageTracker();
  const auto& dict = local_state().GetDict(
      model_execution::prefs::localstate::kLastUsageByFeature);
  EXPECT_EQ(dict.Find("use_case_1"), nullptr);
  EXPECT_NE(dict.Find("use_case_2"), nullptr);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kEvictable);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"), Priority::kRetain);
}

TEST_F(UsageTrackerTest, CustomFeatureParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kOnDeviceModelUsageTracking,
        {{"recent_use_period", "10d"}, {"retention_period", "20d"}}},
       {kOnDeviceModelEviction, {}}},
      {});

  usage_tracker().RaisePriority("use_case_1", Priority::kBestEffort);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kBestEffort);

  task_environment().FastForwardBy(base::Days(11));
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kRetain);

  task_environment().FastForwardBy(base::Days(10));
  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), Priority::kEvictable);

  ResetUsageTracker();
  const auto& dict = local_state().GetDict(
      model_execution::prefs::localstate::kLastUsageByFeature);
  EXPECT_EQ(dict.Find("use_case_1"), nullptr);
}

TEST_F(UsageTrackerTest, EvictionFeatureFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kOnDeviceModelEviction);

  // When kOnDeviceModelEviction is enabled, unused use cases are kEvictable.
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), Priority::kEvictable);

  usage_tracker().RaisePriority("test_use_case", Priority::kBestEffort);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kBestEffort);

  task_environment().FastForwardBy(base::Days(31));
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), Priority::kRetain);

  task_environment().FastForwardBy(base::Days(60));
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), Priority::kEvictable);
}

}  // namespace
}  // namespace optimization_guide
