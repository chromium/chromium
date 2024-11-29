// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/limited_entropy_synthetic_trial.h"

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
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
  base::HistogramTester histogram_tester_;
};

TEST_F(LimitedEntropySyntheticTrialTest,
       GeneratesAndRandomizesWithNewSeed_Stable) {
  ASSERT_FALSE(local_state_.HasPrefPath(
      prefs::kVariationsLimitedEntropySyntheticTrialSeed));

  LimitedEntropySyntheticTrial trial(&local_state_,
                                     version_info::Channel::STABLE);
  auto group_name = trial.GetGroupName();

  // All stable clients must be in the enabled group.
  EXPECT_EQ(kLimitedEntropySyntheticTrialEnabled, group_name);
}

TEST_F(LimitedEntropySyntheticTrialTest,
       GeneratesAndRandomizesWithNewSeed_Prestable) {
  ASSERT_FALSE(local_state_.HasPrefPath(
      prefs::kVariationsLimitedEntropySyntheticTrialSeed));

  LimitedEntropySyntheticTrial trial(&local_state_,
                                     version_info::Channel::BETA);
  auto group_name = trial.GetGroupName();

  // All pre-stable clients must be in the enabled group.
  EXPECT_EQ(kLimitedEntropySyntheticTrialEnabled, group_name);
}

}  // namespace variations
