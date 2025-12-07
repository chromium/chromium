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

class BoostManagerTest : public testing::Test {
 protected:
  base::SimpleTestTickClock test_tick_clock_;
  BoostManager manager_;

  static constexpr base::TimeDelta kTargetFrameDuration =
      base::Milliseconds(10);
  static constexpr base::TimeDelta kActualFrameDuration = base::Milliseconds(7);

  void ExecuteAndAssert(BoostType provided_boost_type,
                        base::TimeTicks provided_draw_start,
                        base::TimeDelta expected_frame_duration) {
    base::TimeDelta frame_duration =
        manager_.GetFrameDuration(kTargetFrameDuration, kActualFrameDuration,
                                  provided_draw_start, provided_boost_type);
    ASSERT_EQ(frame_duration, expected_frame_duration);
  }
};

TEST_F(BoostManagerTest, WakeUpBoostExperimentDisabled) {
  base::TimeTicks draw_start = test_tick_clock_.NowTicks();

  // Use BoostType::kWakeUpBoost, it only increase actual duration.
  ExecuteAndAssert(BoostType::kWakeUpBoost, draw_start,
                   1.5 * kTargetFrameDuration);
  ExecuteAndAssert(BoostType::kDefault, draw_start, kActualFrameDuration);
}

}  // namespace
}  // namespace viz
