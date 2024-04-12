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

TEST_F(LimitedEntropySyntheticTrialTest, RandomizesWithExistingSeed_Enabled) {
  local_state_.SetUint64(prefs::kVariationsLimitedEntropySyntheticTrialSeed,
                         10);
  LimitedEntropySyntheticTrial trial(&local_state_);
  ASSERT_TRUE(trial.IsEnabled());
  ASSERT_EQ(10u, local_state_.GetUint64(
                     prefs::kVariationsLimitedEntropySyntheticTrialSeed));
}

TEST_F(LimitedEntropySyntheticTrialTest, GeneratesAndRandomizesWithNewSeed) {
  ASSERT_FALSE(local_state_.HasPrefPath(
      prefs::kVariationsLimitedEntropySyntheticTrialSeed));

  LimitedEntropySyntheticTrial trial(&local_state_);
  auto group_name = trial.GetGroupName();

  // The client must be in the enabled group because `kEnabledPercentage` is set
  // to 100.
  EXPECT_EQ(kLimitedEntropySyntheticTrialEnabled, group_name);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(LimitedEntropySyntheticTrialTest, TestSetSeedFromAsh) {
  LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 42u);
  LimitedEntropySyntheticTrial trial(&local_state_);

  EXPECT_EQ(42u, trial.GetRandomizationSeed(&local_state_));
  histogram_tester_.ExpectUniqueSample(
      kIsLimitedEntropySyntheticTrialSeedValidHistogram, true, 1);
}

TEST_F(LimitedEntropySyntheticTrialTest,
       TestSetSeedFromAsh_ExpectCheckIFailureIfRandomizedBeforeSyncingSeed) {
  LimitedEntropySyntheticTrial trial(&local_state_);
  EXPECT_CHECK_DEATH(
      LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 42u));
}

TEST_F(
    LimitedEntropySyntheticTrialTest,
    TestSetSeedFromAsh_ExpectCheckIFailureIfSettingSeedAgainAfterRandomization) {
  LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 42u);
  LimitedEntropySyntheticTrial trial(&local_state_);
  EXPECT_CHECK_DEATH(
      LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 62u));
  histogram_tester_.ExpectUniqueSample(
      kIsLimitedEntropySyntheticTrialSeedValidHistogram, true, 1);
}

TEST_F(LimitedEntropySyntheticTrialTest,
       TestSetSeedFromAsh_SyncingInvalidSeed) {
  LimitedEntropySyntheticTrial::SetSeedFromAsh(&local_state_, 999u);
  LimitedEntropySyntheticTrial trial(&local_state_);
  EXPECT_NE(999u, trial.GetRandomizationSeed(&local_state_));
  histogram_tester_.ExpectUniqueSample(
      kIsLimitedEntropySyntheticTrialSeedValidHistogram, false, 1);
}

#endif
}  // namespace variations
