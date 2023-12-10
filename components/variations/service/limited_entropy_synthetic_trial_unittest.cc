// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/limited_entropy_synthetic_trial.h"

#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

class LimitedEntropySyntheticTrialTest : public ::testing::Test {
 public:
  LimitedEntropySyntheticTrialTest() {
    LimitedEntropySyntheticTrial::RegisterPrefs(local_state_.registry());
  }

 protected:
  TestingPrefServiceSimple local_state_;
};

TEST_F(LimitedEntropySyntheticTrialTest, RandomizesWithExistingSeed_Enabled) {
  local_state_.SetUint64(prefs::kVariationsLimitedEntropySyntheticTrialSeed,
                         10);
  LimitedEntropySyntheticTrial trial(&local_state_);
  ASSERT_TRUE(trial.IsEnabled());
  ASSERT_EQ(10u, local_state_.GetUint64(
                     prefs::kVariationsLimitedEntropySyntheticTrialSeed));
}

TEST_F(LimitedEntropySyntheticTrialTest, RandomizesWithExistingSeed_Disabled) {
  local_state_.SetUint64(prefs::kVariationsLimitedEntropySyntheticTrialSeed,
                         90);
  LimitedEntropySyntheticTrial trial(&local_state_);
  ASSERT_FALSE(trial.IsEnabled());
  ASSERT_EQ(90u, local_state_.GetUint64(
                     prefs::kVariationsLimitedEntropySyntheticTrialSeed));
}

TEST_F(LimitedEntropySyntheticTrialTest, GeneratesAndRandomizesWithNewSeed) {
  ASSERT_FALSE(local_state_.HasPrefPath(
      prefs::kVariationsLimitedEntropySyntheticTrialSeed));

  LimitedEntropySyntheticTrial trial(&local_state_);
  auto group_name = trial.GetGroupName();

  // The default group should not be activated when 50% of the population is in
  // the enabled group.
  ASSERT_NE(kLimitedEntropySyntheticTrialDefault, group_name);
  auto is_enabled = group_name == kLimitedEntropySyntheticTrialEnabled;
  auto is_control = group_name == kLimitedEntropySyntheticTrialControl;
  ASSERT_TRUE(is_enabled || is_control);

  auto rand_seed = local_state_.GetUint64(
      prefs::kVariationsLimitedEntropySyntheticTrialSeed);
  if (rand_seed < 50u) {
    ASSERT_TRUE(is_enabled);
  } else {
    ASSERT_LT(rand_seed, 100u);
    ASSERT_TRUE(is_control);
  }
}

}  // namespace variations
