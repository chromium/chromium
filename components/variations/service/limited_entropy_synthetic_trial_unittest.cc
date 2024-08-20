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

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(LimitedEntropySyntheticTrialTest, TestSetSeedFromAsh) {
  LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 42u);
  LimitedEntropySyntheticTrial trial(&local_state_,
                                     version_info::Channel::BETA);

  EXPECT_EQ(42u, trial.GetRandomizationSeed(&local_state_));
  histogram_tester_.ExpectUniqueSample(
      kIsLimitedEntropySyntheticTrialSeedValidHistogram, true, 1);
}

TEST_F(LimitedEntropySyntheticTrialTest,
       TestSetSeedFromAsh_ExpectCheckIFailureIfRandomizedBeforeSyncingSeed) {
  LimitedEntropySyntheticTrial trial(&local_state_,
                                     version_info::Channel::BETA);
  EXPECT_CHECK_DEATH(
      LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 42u));
}

TEST_F(
    LimitedEntropySyntheticTrialTest,
    TestSetSeedFromAsh_ExpectCheckIFailureIfSettingSeedAgainAfterRandomization) {
  LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 42u);
  LimitedEntropySyntheticTrial trial(&local_state_,
                                     version_info::Channel::BETA);
  EXPECT_CHECK_DEATH(
      LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 62u));
  histogram_tester_.ExpectUniqueSample(
      kIsLimitedEntropySyntheticTrialSeedValidHistogram, true, 1);
}

TEST_F(LimitedEntropySyntheticTrialTest,
       TestSetSeedFromAsh_SyncingInvalidSeed) {
  LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 999u);
  LimitedEntropySyntheticTrial trial(&local_state_,
                                     version_info::Channel::BETA);
  EXPECT_NE(999u, trial.GetRandomizationSeed(&local_state_));
  histogram_tester_.ExpectUniqueSample(
      kIsLimitedEntropySyntheticTrialSeedValidHistogram, false, 1);
}

#endif
}  // namespace variations
