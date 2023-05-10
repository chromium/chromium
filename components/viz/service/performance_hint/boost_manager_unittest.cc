// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/boost_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/service/performance_hint/hint_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {
using BoostType = HintSession::BoostType;

class BoostManagerTestHelper : public BoostManager {
 public:
  using BoostManager::GetCurrentBoostTypeForTesting;
};

}  // namespace

class BoostManagerTest : public testing::Test {
 private:
  base::test::ScopedFeatureList scoped_list_;

 protected:
  base::SimpleTestTickClock test_tick_clock_;
  BoostManagerTestHelper manager_;

  static constexpr base::TimeDelta kTargetFrameDuration =
      base::Milliseconds(10);
  static constexpr base::TimeDelta kActualFrameDuration = base::Milliseconds(7);

  void InitAndEnableBoostFeature() {
    scoped_list_.InitAndEnableFeature(features::kEnableADPFScrollBoost);
  }

  void InitAndDisableBoostFeature() {
    scoped_list_.InitAndDisableFeature(features::kEnableADPFScrollBoost);
  }

  base::TimeDelta GetBoostModeTimeout() const {
    return features::kADPFBoostTimeout.Get();
  }

  void ExecuteAndAssert(BoostType provided_boost_type,
                        base::TimeTicks provided_draw_start,
                        BoostType expected_boost_type,
                        base::TimeDelta expected_frame_duration) {
    base::TimeDelta frame_duration =
        manager_.GetFrameDurationAndMaybeUpdateBoostType(
            kTargetFrameDuration, kActualFrameDuration, provided_draw_start,
            provided_boost_type);
    ASSERT_EQ(frame_duration, expected_frame_duration);
    BoostType new_boost_type = manager_.GetCurrentBoostTypeForTesting();
    ASSERT_EQ(new_boost_type, expected_boost_type);
  }
};

TEST_F(BoostManagerTest, ScrollBoostExperimentEnabled) {
  InitAndEnableBoostFeature();

  base::TimeTicks draw_start = test_tick_clock_.NowTicks();

  // Use BoostType::kDefault.
  ExecuteAndAssert(BoostType::kDefault, draw_start, BoostType::kDefault,
                   kActualFrameDuration);

  // Use BoostType::kScrollBoost, it takes effect immediately.
  ExecuteAndAssert(BoostType::kScrollBoost, draw_start, BoostType::kScrollBoost,
                   3 * kTargetFrameDuration);

  // Use BoostType::kDefault, but it doesn't take effect, because the previously
  // set BoostType::kScrollBoost has not yet been exhausted.
  ExecuteAndAssert(BoostType::kDefault, draw_start, BoostType::kScrollBoost,
                   3 * kTargetFrameDuration);

  // Use BoostType::kDefault, after BoostType::kScrollBoost has been exhausted.
  test_tick_clock_.Advance(GetBoostModeTimeout() + base::Milliseconds(1));
  draw_start = test_tick_clock_.NowTicks();
  ExecuteAndAssert(BoostType::kDefault, draw_start, BoostType::kDefault,
                   kActualFrameDuration);
}

TEST_F(BoostManagerTest, ScrollBoostExperimentDisabled) {
  InitAndDisableBoostFeature();

  base::TimeTicks draw_start = test_tick_clock_.NowTicks();

  // Use BoostType::kDefault.
  ExecuteAndAssert(BoostType::kDefault, draw_start, BoostType::kDefault,
                   kActualFrameDuration);

  // Use BoostType::kScrollBoost, it doesn't take effect because the experiment
  // is disabled.
  ExecuteAndAssert(BoostType::kScrollBoost, draw_start, BoostType::kDefault,
                   kActualFrameDuration);
}

TEST_F(BoostManagerTest, WakeUpBoostExperimentEnabled) {
  InitAndEnableBoostFeature();

  base::TimeTicks draw_start = test_tick_clock_.NowTicks();

  // Use BoostType::kWakeUpBoost, it takes effect immediately.
  ExecuteAndAssert(BoostType::kWakeUpBoost, draw_start, BoostType::kWakeUpBoost,
                   1.5 * kTargetFrameDuration);

  // Use BoostType::kDefault, but it doesn't take effect, because the previously
  // set BoostType::kWakeUpBoost has not yet been exhausted.
  ExecuteAndAssert(BoostType::kDefault, draw_start, BoostType::kWakeUpBoost,
                   1.5 * kTargetFrameDuration);

  // Use BoostType::kDefault, after BoostType::kWakeUpBoost has been exhausted.
  test_tick_clock_.Advance(GetBoostModeTimeout() + base::Milliseconds(1));
  draw_start = test_tick_clock_.NowTicks();
  ExecuteAndAssert(BoostType::kDefault, draw_start, BoostType::kDefault,
                   kActualFrameDuration);
}

TEST_F(BoostManagerTest, WakeUpBoostExperimentDisabled) {
  InitAndDisableBoostFeature();

  base::TimeTicks draw_start = test_tick_clock_.NowTicks();

  // Use BoostType::kWakeUpBoost, it only increase actual duration(the same
  // behaviour as before introducing this experiment) because the experiment is
  // disabled.
  ExecuteAndAssert(BoostType::kWakeUpBoost, draw_start, BoostType::kDefault,
                   1.5 * kTargetFrameDuration);
  ExecuteAndAssert(BoostType::kDefault, draw_start, BoostType::kDefault,
                   kActualFrameDuration);
}

}  // namespace viz
