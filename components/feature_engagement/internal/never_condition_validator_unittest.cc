// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_condition_validator.h"

#include <string>

#include "base/feature_list.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/noop_display_lock_controller.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/test/test_time_provider.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kNeverTestFeatureFoo,
             "test_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNeverTestFeatureBar,
             "test_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A EventModel that is always postive to show in-product help.
class NeverTestEventModel : public EventModel {
 public:
  NeverTestEventModel() = default;

  NeverTestEventModel(const NeverTestEventModel&) = delete;
  NeverTestEventModel& operator=(const NeverTestEventModel&) = delete;

  void Initialize(OnModelInitializationFinished callback,
                  uint32_t current_day) override {}

  bool IsReady() const override { return true; }

  const Event* GetEvent(const std::string& event_name) const override {
    return nullptr;
  }

  uint32_t GetEventCount(const std::string& event_name,
                         uint32_t current_day,
                         uint32_t window_size) const override {
    return 0;
  }

  void IncrementEvent(const std::string& event_name, uint32_t day) override {}

  void ClearEvent(const std::string& event_name) override {}

  void IncrementSnooze(const std::string& event_name,
                       uint32_t day,
                       base::Time time) override {}

  void DismissSnooze(const std::string& event_name) override {}

  base::Time GetLastSnoozeTimestamp(
      const std::string& event_name) const override {
    return base::Time();
  }

  uint32_t GetSnoozeCount(const std::string& event_name,
                          uint32_t window,
                          uint32_t current_day) const override {
    return 0;
  }

  bool IsSnoozeDismissed(const std::string& event_name) const override {
    return false;
  }
};

class NeverConditionValidatorTest : public ::testing::Test {
 public:
  NeverConditionValidatorTest() = default;

  NeverConditionValidatorTest(const NeverConditionValidatorTest&) = delete;
  NeverConditionValidatorTest& operator=(const NeverConditionValidatorTest&) =
      delete;

 protected:
  NeverTestEventModel event_model_;
  NeverAvailabilityModel availability_model_;
  NoopDisplayLockController display_lock_controller_;
  NeverConditionValidator validator_;
  TestTimeProvider time_provider_;
};

}  // namespace

TEST_F(NeverConditionValidatorTest, ShouldNeverMeetConditions) {
  EXPECT_FALSE(validator_
                   .MeetsConditions(kNeverTestFeatureFoo, FeatureConfig(), {},
                                    event_model_, availability_model_,
                                    display_lock_controller_, nullptr,
                                    time_provider_)
                   .NoErrors());
  EXPECT_FALSE(validator_
                   .MeetsConditions(kNeverTestFeatureBar, FeatureConfig(), {},
                                    event_model_, availability_model_,
                                    display_lock_controller_, nullptr,
                                    time_provider_)
                   .NoErrors());
  EXPECT_FALSE(validator_.GetPendingPriorityNotification().has_value());
}

}  // namespace feature_engagement
