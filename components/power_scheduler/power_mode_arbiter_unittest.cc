// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_arbiter.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_scheduler {

TEST(PowerModeArbiterTest, SingleVote) {
  PowerModeArbiter arbiter;
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);

  // Clear the initial kCharging vote.
  arbiter.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  std::unique_ptr<PowerModeVoter> voter1 = arbiter.NewVoter("voter1");
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  for (PowerMode mode = PowerMode::kIdle; mode <= PowerMode::kMaxValue;) {
    mode = static_cast<PowerMode>(static_cast<int>(mode) + 1);
    voter1->VoteFor(mode);
    EXPECT_EQ(arbiter.GetActiveModeForTesting(), mode);
  }

  voter1.reset();
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);
}

TEST(PowerModeArbiterTest, MultipleVotes) {
  PowerModeArbiter arbiter;
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);

  // Clear the initial kCharging vote.
  arbiter.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  std::unique_ptr<PowerModeVoter> voter1 = arbiter.NewVoter("voter1");
  std::unique_ptr<PowerModeVoter> voter2 = arbiter.NewVoter("voter2");
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  auto vote_and_expect = [&](PowerMode vote1, PowerMode vote2,
                             PowerMode expected) {
    voter1->VoteFor(vote1);
    voter2->VoteFor(vote2);
    EXPECT_EQ(arbiter.GetActiveModeForTesting(), expected)
        << "vote1: " << PowerModeToString(vote1)
        << ", vote2: " << PowerModeToString(vote2)
        << ", expected: " << PowerModeToString(expected)
        << ", actual: " << PowerModeToString(arbiter.GetActiveModeForTesting());
  };

  // Two votes for the same mode result in that mode.
  for (PowerMode mode = PowerMode::kIdle; mode <= PowerMode::kMaxValue;) {
    vote_and_expect(mode, mode, mode);
    mode = static_cast<PowerMode>(static_cast<int>(mode) + 1);
  }

  // Charging trumps anything.
  vote_and_expect(PowerMode::kCharging, PowerMode::kIdle, PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kNopAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kAudible,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kVideoPlayback,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kMainThreadAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kLoading,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kLoadingAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kResponse,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kNonWebActivity,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kBackground,
                  PowerMode::kCharging);

  // Background trumps remaining modes, but not audible.
  vote_and_expect(PowerMode::kBackground, PowerMode::kIdle,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kNopAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kAudible,
                  PowerMode::kAudible);
  vote_and_expect(PowerMode::kBackground, PowerMode::kVideoPlayback,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kMainThreadAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kLoading,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kLoadingAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kResponse,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kNonWebActivity,
                  PowerMode::kBackground);

  // NonWebActivity trumps remaining modes.
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kIdle,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kNopAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kAudible,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kVideoPlayback,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kMainThreadAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kLoading,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kLoadingAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kResponse,
                  PowerMode::kNonWebActivity);

  // Response trumps remaining modes.
  vote_and_expect(PowerMode::kResponse, PowerMode::kIdle, PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kNopAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kAudible,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kVideoPlayback,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kMainThreadAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kLoading,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kLoadingAnimation,
                  PowerMode::kResponse);

  // LoadingAnimation trumps remaining modes.
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kIdle,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kNopAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kAudible,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kVideoPlayback,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kMainThreadAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kLoading,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kAnimation,
                  PowerMode::kLoadingAnimation);

  // Animation trumps remaining modes.
  vote_and_expect(PowerMode::kAnimation, PowerMode::kIdle,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kNopAnimation,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kAudible,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kVideoPlayback,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kMainThreadAnimation,
                  PowerMode::kAnimation);
  // Animation while loading breaks out into a separate mode.
  vote_and_expect(PowerMode::kAnimation, PowerMode::kLoading,
                  PowerMode::kLoadingAnimation);

  // Loading trumps remaining modes.
  vote_and_expect(PowerMode::kLoading, PowerMode::kIdle, PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kNopAnimation,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kAudible,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kVideoPlayback,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kMainThreadAnimation,
                  PowerMode::kLoading);

  // MainThreadAnimation trumps remaining modes.
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kIdle,
                  PowerMode::kMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kNopAnimation,
                  PowerMode::kMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kAudible,
                  PowerMode::kMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kVideoPlayback,
                  PowerMode::kMainThreadAnimation);

  // VideoPlayback trumps remaining modes.
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kIdle,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kNopAnimation,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kAudible,
                  PowerMode::kVideoPlayback);

  // Audible trumps idle and no-op animation.
  vote_and_expect(PowerMode::kAudible, PowerMode::kIdle, PowerMode::kAudible);
  vote_and_expect(PowerMode::kAudible, PowerMode::kNopAnimation,
                  PowerMode::kAudible);

  // NopAnimation trumps idle.
  vote_and_expect(PowerMode::kNopAnimation, PowerMode::kIdle,
                  PowerMode::kNopAnimation);
}

namespace {
class MockObserver : public PowerModeArbiter::Observer {
 public:
  ~MockObserver() override = default;
  MOCK_METHOD(void,
              OnPowerModeChanged,
              (PowerMode old_mode, PowerMode new_mode),
              (override));
};
}  // namespace

TEST(PowerModeArbiterTest, Observer) {
  base::test::TaskEnvironment env;
  PowerModeArbiter arbiter;
  MockObserver observer;

  // Clear the initial kCharging vote.
  arbiter.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);

  // Observer is notified of initial mode right away.
  EXPECT_CALL(observer, OnPowerModeChanged(PowerMode::kIdle, PowerMode::kIdle));

  arbiter.AddObserver(&observer);
  testing::Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<PowerModeVoter> voter1 = arbiter.NewVoter("voter1");
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  EXPECT_CALL(observer,
              OnPowerModeChanged(PowerMode::kIdle, PowerMode::kAnimation));

  voter1->VoteFor(PowerMode::kAnimation);
  env.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnPowerModeChanged(PowerMode::kAnimation, PowerMode::kIdle));

  voter1.reset();
  env.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&observer);

  arbiter.RemoveObserver(&observer);
}

namespace {
class FakeObserver : public PowerModeArbiter::Observer {
 public:
  ~FakeObserver() override = default;
  void OnPowerModeChanged(PowerMode old_mode, PowerMode new_mode) override {}
};
}  // namespace

TEST(PowerModeArbiterTest, ResetVoteAfterTimeout) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Align the mock clock with the phase of the reset tasks.
  base::TimeTicks target_time = env.NowTicks().SnappedToNextTick(
      base::TimeTicks(), PowerModeArbiter::kResetVoteTimeResolution);
  env.AdvanceClock(target_time - env.NowTicks());

  PowerModeArbiter arbiter;
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);

  // Clear the initial kCharging vote.
  arbiter.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  // Add a fake observer to enable reset tasks.
  FakeObserver observer;
  arbiter.AddObserver(&observer);

  base::TimeDelta delta1s = base::TimeDelta::FromSeconds(1);
  base::TimeDelta delta2s = base::TimeDelta::FromSeconds(2);

  std::unique_ptr<PowerModeVoter> voter1 = arbiter.NewVoter("voter1");
  voter1->VoteFor(PowerMode::kAnimation);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);

  // Reset requests before the thread pool is available are queued and executed
  // on thread pool becoming available.
  voter1->ResetVoteAfterTimeout(delta1s);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  env.FastForwardBy(delta1s);  // Advance the time before the task is queued.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);

  arbiter.OnThreadPoolAvailable();
  env.RunUntilIdle();  // Execute the (non-delayed) reset task.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  // If VoteFor() is not called before the task executes, the mode is reset.
  voter1->VoteFor(PowerMode::kAnimation);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  voter1->ResetVoteAfterTimeout(delta1s);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  env.FastForwardBy(delta1s);  // Execute the reset task.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  // If the VoteFor() is called before the task executes, the mode is not reset.
  voter1->VoteFor(PowerMode::kAnimation);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  voter1->ResetVoteAfterTimeout(delta1s);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  voter1->VoteFor(PowerMode::kAnimation);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  env.FastForwardBy(delta1s);  // Execute the reset task.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);

  // Handles multiple pending resets.
  voter1->VoteFor(PowerMode::kAnimation);
  std::unique_ptr<PowerModeVoter> voter2 = arbiter.NewVoter("voter2");
  voter2->VoteFor(PowerMode::kCharging);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);
  voter2->ResetVoteAfterTimeout(delta1s);
  voter1->ResetVoteAfterTimeout(delta2s);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);
  env.FastForwardBy(delta1s);  // Execute the first reset task.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  env.FastForwardBy(delta1s);  // Execute the second reset task.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  // Same thing, with reset requests scheduled in reverse order.
  voter1->VoteFor(PowerMode::kAnimation);
  voter2->VoteFor(PowerMode::kCharging);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);
  voter1->ResetVoteAfterTimeout(delta2s);
  voter2->ResetVoteAfterTimeout(delta1s);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);
  env.FastForwardBy(delta1s);  // Execute the first reset task.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
  env.FastForwardBy(delta1s);  // Execute the second reset task.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  // Unaligned reset timeouts get aligned to the resolution.
  voter1->VoteFor(PowerMode::kAnimation);
  voter2->VoteFor(PowerMode::kCharging);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);
  voter2->ResetVoteAfterTimeout(PowerModeArbiter::kResetVoteTimeResolution / 3);
  voter1->ResetVoteAfterTimeout(PowerModeArbiter::kResetVoteTimeResolution / 2);
  base::TimeDelta first_half = PowerModeArbiter::kResetVoteTimeResolution / 2;
  env.FastForwardBy(first_half);
  // No change, since the timeouts were aligned to kResetVoteTimeResolution.
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);
  // Executes the resets.
  env.FastForwardBy(PowerModeArbiter::kResetVoteTimeResolution - first_half);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  // If the voter is destroyed, the task doesn't cause crashes.
  voter1->VoteFor(PowerMode::kAnimation);
  voter1->ResetVoteAfterTimeout(delta1s);
  voter1.reset();
  env.FastForwardBy(delta1s);  // Execute the reset task.

  arbiter.RemoveObserver(&observer);
}

TEST(PowerModeArbiterTest, ObserverEnablesResetTasks) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Align the mock clock with the phase of the reset tasks.
  base::TimeTicks target_time = env.NowTicks().SnappedToNextTick(
      base::TimeTicks(), PowerModeArbiter::kResetVoteTimeResolution);
  env.AdvanceClock(target_time - env.NowTicks());

  PowerModeArbiter arbiter;
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);

  // Clear the initial kCharging vote.
  arbiter.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

  FakeObserver observer;
  base::TimeDelta delta1s = base::TimeDelta::FromSeconds(1);

  arbiter.OnThreadPoolAvailable();

  std::unique_ptr<PowerModeVoter> voter1 = arbiter.NewVoter("voter1");

  for (int i = 0; i < 2; i++) {
    // Without observer, reset tasks are not executed and resets not serviced.
    voter1->VoteFor(PowerMode::kAnimation);
    EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter1->ResetVoteAfterTimeout(delta1s);
    {
      base::AutoLock lock(arbiter.lock_);
      EXPECT_EQ(arbiter.next_pending_vote_update_time_, base::TimeTicks());
    }
    env.FastForwardBy(delta1s);
    EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);

    // Adding the observer services the reset.
    arbiter.AddObserver(&observer);
    EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

    // While observer is registered, resets are serviced.
    voter1->VoteFor(PowerMode::kAnimation);
    EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter1->ResetVoteAfterTimeout(delta1s);
    {
      base::AutoLock lock(arbiter.lock_);
      EXPECT_NE(arbiter.next_pending_vote_update_time_, base::TimeTicks());
    }
    env.FastForwardBy(delta1s);
    EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kIdle);

    // After removing the observer, resets are no longer serviced.
    arbiter.RemoveObserver(&observer);
  }
}

}  // namespace power_scheduler