// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/usage_tracker.h"

#include <memory>
#include <optional>
#include <string>

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
              (const std::string& use_case_name,
               std::optional<Priority> previous_priority),
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

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<UsageTracker> usage_tracker_;
};

TEST_F(UsageTrackerTest, GetPriorityUserBlocking) {
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), std::nullopt);

  usage_tracker().RaisePriority("test_use_case", Priority::kUserBlocking);

  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kUserBlocking);
}

TEST_F(UsageTrackerTest, GetPriorityBestEffort) {
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), std::nullopt);

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
  usage_tracker().SetPriority("test_use_case", Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kUserBlocking);

  usage_tracker().SetPriority("test_use_case", std::nullopt);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"), std::nullopt);
}

TEST_F(UsageTrackerTest, SetPriorityBestEffort) {
  usage_tracker().SetPriority("test_use_case", Priority::kBestEffort);
  EXPECT_EQ(usage_tracker().GetPriority("test_use_case"),
            Priority::kBestEffort);
}

TEST_F(UsageTrackerTest, ObserverNotified) {
  MockUsageTrackerObserver observer;
  usage_tracker().AddObserver(&observer);

  EXPECT_CALL(observer,
              OnPriorityIncrease("test_use_case", std::optional<Priority>()))
      .Times(1);
  usage_tracker().RaisePriority("test_use_case", Priority::kBestEffort);

  EXPECT_CALL(observer,
              OnPriorityIncrease("test_use_case",
                                 std::optional<Priority>(Priority::kBestEffort)))
      .Times(1);
  usage_tracker().RaisePriority("test_use_case", Priority::kUserBlocking);

  // Calling RaisePriority again with same or lower priority does not fire observer.
  EXPECT_CALL(observer, OnPriorityIncrease).Times(0);
  usage_tracker().RaisePriority("test_use_case", Priority::kUserBlocking);
  usage_tracker().RaisePriority("test_use_case", Priority::kBestEffort);

  usage_tracker().RemoveObserver(&observer);
}

TEST_F(UsageTrackerTest, ClearAllUseCaseUsages) {
  usage_tracker().RaisePriority("use_case_1", Priority::kUserBlocking);
  usage_tracker().RaisePriority("use_case_2", Priority::kBestEffort);

  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"),
            Priority::kUserBlocking);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"),
            Priority::kBestEffort);

  usage_tracker().ClearAllUseCaseUsages();

  EXPECT_EQ(usage_tracker().GetPriority("use_case_1"), std::nullopt);
  EXPECT_EQ(usage_tracker().GetPriority("use_case_2"), std::nullopt);
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

  EXPECT_CALL(observer, OnPriorityIncrease(scam_detection_use_case,
                                           std::optional<Priority>()))
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

}  // namespace
}  // namespace optimization_guide
