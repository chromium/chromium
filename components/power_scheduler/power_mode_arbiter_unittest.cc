// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_arbiter.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
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

TEST(PowerModeArbiterTest, DisableCharging) {
  PowerModeArbiter arbiter;
  EXPECT_EQ(arbiter.GetActiveModeForTesting(), PowerMode::kCharging);

  arbiter.SetChargingModeEnabled(false);
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
  vote_and_expect(PowerMode::kCharging, PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kSmallAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kMediumAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kAudible,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kVideoPlayback,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kMainThreadAnimation,
                  PowerMode::kCharging);
  vote_and_expect(PowerMode::kCharging, PowerMode::kScriptExecution,
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
  vote_and_expect(PowerMode::kBackground, PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kSmallAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kMediumAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kAudible,
                  PowerMode::kAudible);
  vote_and_expect(PowerMode::kBackground, PowerMode::kVideoPlayback,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kMainThreadAnimation,
                  PowerMode::kBackground);
  vote_and_expect(PowerMode::kBackground, PowerMode::kScriptExecution,
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
  vote_and_expect(PowerMode::kNonWebActivity,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kSmallAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity,
                  PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kMediumAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kAudible,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kVideoPlayback,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kMainThreadAnimation,
                  PowerMode::kNonWebActivity);
  vote_and_expect(PowerMode::kNonWebActivity, PowerMode::kScriptExecution,
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
  vote_and_expect(PowerMode::kResponse, PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kSmallAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kMediumAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kAudible,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kVideoPlayback,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kMainThreadAnimation,
                  PowerMode::kResponse);
  vote_and_expect(PowerMode::kResponse, PowerMode::kScriptExecution,
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
  vote_and_expect(PowerMode::kLoadingAnimation,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kSmallAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation,
                  PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kMediumAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kAudible,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kVideoPlayback,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kMainThreadAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoadingAnimation, PowerMode::kScriptExecution,
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
  vote_and_expect(PowerMode::kAnimation, PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kSmallAnimation,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kMediumAnimation,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kAudible,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kVideoPlayback,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kMainThreadAnimation,
                  PowerMode::kAnimation);
  vote_and_expect(PowerMode::kAnimation, PowerMode::kScriptExecution,
                  PowerMode::kAnimation);
  // Animation while loading breaks out into a separate mode.
  vote_and_expect(PowerMode::kAnimation, PowerMode::kLoading,
                  PowerMode::kLoadingAnimation);

  // Loading trumps remaining modes, but loading while small/medium animations
  // breaks out into LoadingAnimation.
  vote_and_expect(PowerMode::kLoading, PowerMode::kIdle, PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kNopAnimation,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kSmallAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoading, PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kMediumAnimation,
                  PowerMode::kLoadingAnimation);
  vote_and_expect(PowerMode::kLoading, PowerMode::kAudible,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kVideoPlayback,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kMainThreadAnimation,
                  PowerMode::kLoading);
  vote_and_expect(PowerMode::kLoading, PowerMode::kScriptExecution,
                  PowerMode::kLoading);

  // Script execution trumps remaining modes.
  vote_and_expect(PowerMode::kScriptExecution, PowerMode::kIdle,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution, PowerMode::kNopAnimation,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution, PowerMode::kSmallAnimation,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution,
                  PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution, PowerMode::kMediumAnimation,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution, PowerMode::kAudible,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution, PowerMode::kVideoPlayback,
                  PowerMode::kScriptExecution);
  vote_and_expect(PowerMode::kScriptExecution, PowerMode::kMainThreadAnimation,
                  PowerMode::kScriptExecution);

  // MainThreadAnimation trumps remaining modes, except for other animation
  // modes (NopAnimation, SmallAnimation, MediumAnimation), which affect it.
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kIdle,
                  PowerMode::kMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kNopAnimation,
                  PowerMode::kNopAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kSmallAnimation,
                  PowerMode::kSmallMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation,
                  PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kMediumAnimation,
                  PowerMode::kMediumMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kAudible,
                  PowerMode::kMainThreadAnimation);
  vote_and_expect(PowerMode::kMainThreadAnimation, PowerMode::kVideoPlayback,
                  PowerMode::kMainThreadAnimation);

  // VideoPlayback trumps remaining modes.
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kIdle,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kNopAnimation,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kSmallAnimation,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback,
                  PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kMediumAnimation,
                  PowerMode::kVideoPlayback);
  vote_and_expect(PowerMode::kVideoPlayback, PowerMode::kAudible,
                  PowerMode::kVideoPlayback);

  // Audible trumps remaining modes.
  vote_and_expect(PowerMode::kAudible, PowerMode::kIdle, PowerMode::kAudible);
  vote_and_expect(PowerMode::kAudible, PowerMode::kNopAnimation,
                  PowerMode::kAudible);
  vote_and_expect(PowerMode::kAudible, PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kAudible);
  vote_and_expect(PowerMode::kAudible, PowerMode::kSmallAnimation,
                  PowerMode::kAudible);
  vote_and_expect(PowerMode::kAudible, PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kAudible);
  vote_and_expect(PowerMode::kAudible, PowerMode::kMediumAnimation,
                  PowerMode::kAudible);

  // MediumAnimation trumps remaining modes.
  vote_and_expect(PowerMode::kMediumAnimation, PowerMode::kIdle,
                  PowerMode::kMediumAnimation);
  vote_and_expect(PowerMode::kMediumAnimation, PowerMode::kNopAnimation,
                  PowerMode::kMediumAnimation);
  vote_and_expect(PowerMode::kMediumAnimation,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kMediumAnimation);
  vote_and_expect(PowerMode::kMediumAnimation, PowerMode::kSmallAnimation,
                  PowerMode::kMediumAnimation);
  vote_and_expect(PowerMode::kMediumAnimation,
                  PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kMediumAnimation);

  // MediumMainThreadAnimation trumps remaining modes.
  vote_and_expect(PowerMode::kMediumMainThreadAnimation, PowerMode::kIdle,
                  PowerMode::kMediumMainThreadAnimation);
  vote_and_expect(PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kNopAnimation,
                  PowerMode::kMediumMainThreadAnimation);
  vote_and_expect(PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kMediumMainThreadAnimation);
  vote_and_expect(PowerMode::kMediumMainThreadAnimation,
                  PowerMode::kSmallAnimation,
                  PowerMode::kMediumMainThreadAnimation);

  // SmallAnimation trumps remaining modes.
  vote_and_expect(PowerMode::kSmallAnimation, PowerMode::kIdle,
                  PowerMode::kSmallAnimation);
  vote_and_expect(PowerMode::kSmallAnimation, PowerMode::kNopAnimation,
                  PowerMode::kSmallAnimation);
  vote_and_expect(PowerMode::kSmallAnimation,
                  PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kSmallAnimation);

  // SmallMainThreadAnimation trumps remaining modes.
  vote_and_expect(PowerMode::kSmallMainThreadAnimation, PowerMode::kIdle,
                  PowerMode::kSmallMainThreadAnimation);
  vote_and_expect(PowerMode::kSmallMainThreadAnimation,
                  PowerMode::kNopAnimation,
                  PowerMode::kSmallMainThreadAnimation);

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

  base::TimeDelta delta1s = base::Seconds(1);
  base::TimeDelta delta2s = base::Seconds(2);

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
  base::TimeDelta delta1s = base::Seconds(1);

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

class PowerModeArbiterFrameProductionTest : public testing::Test {
 public:
  PowerModeArbiterFrameProductionTest()
      : env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        voter_("voter1", &arbiter_) {
    // Align the mock clock with the phase of the reset tasks.
    base::TimeTicks target_time = env_.NowTicks().SnappedToNextTick(
        base::TimeTicks(), PowerModeArbiter::kResetVoteTimeResolution);
    env_.AdvanceClock(target_time - env_.NowTicks());

    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kCharging);

    // Clear the initial kCharging vote.
    arbiter_.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kIdle);

    arbiter_.AddObserver(&observer_);
    arbiter_.OnThreadPoolAvailable();

    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kIdle);
  }

 protected:
  static constexpr int kNumDamageAreas =
      FrameProductionPowerModeVoter::kNumDamageAreas;
  static constexpr int kMinFramesSkippedForIdleAnimation =
      FrameProductionPowerModeVoter::kMinFramesSkippedForIdleAnimation;
  static constexpr float kDeviceScaleFactor = 1.0f;

  base::test::TaskEnvironment env_;
  PowerModeArbiter arbiter_;
  FakeObserver observer_;
  FrameProductionPowerModeVoter voter_;
};

TEST_F(PowerModeArbiterFrameProductionTest, NeedsBeginFrames) {
  // Enter kAnimation when BeginFrames are newly needed.
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // Return to kIdle after a timeout when BeginFrames are no longer needed.
  voter_.OnNeedsBeginFramesChanged(false);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
  env_.FastForwardBy(PowerModeVoter::kAnimationTimeout);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kIdle);
}

TEST_F(PowerModeArbiterFrameProductionTest, SmallAnimation) {
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // After 9 frames with small damage, move into kSmallAnimation mode.
  for (int i = 0; i < kNumDamageAreas - 1; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);

  // Just one larger frame doesn't move us out of the mode.
  voter_.OnFrameProduced(gfx::Rect(640, 640), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);

  // But a second one does.
  voter_.OnFrameProduced(gfx::Rect(640, 640), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
}

TEST_F(PowerModeArbiterFrameProductionTest, MediumAnimation) {
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // After 9 frames with medium damage, move into kMediumAnimation mode.
  for (int i = 0; i < kNumDamageAreas - 1; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameProduced(gfx::Rect(320, 320), kDeviceScaleFactor);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kMediumAnimation);

  // Just one larger frame doesn't move us out of the mode.
  voter_.OnFrameProduced(gfx::Rect(640, 640), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kMediumAnimation);

  // But a second one does.
  voter_.OnFrameProduced(gfx::Rect(640, 640), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
}

TEST_F(PowerModeArbiterFrameProductionTest,
       SwitchBetweenSmallAndMediumAnimation) {
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // Move into kSmallAnimation.
  for (int i = 0; i < kNumDamageAreas - 1; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);

  // Switch to kMediumAnimation from kSmallAnimation works too.
  voter_.OnFrameProduced(gfx::Rect(320, 320), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);
  voter_.OnFrameProduced(gfx::Rect(320, 320), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kMediumAnimation);

  // And switching back to kSmallAnimation also works.
  for (int i = 0; i < kNumDamageAreas - 1; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kMediumAnimation);
    voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);
}

TEST_F(PowerModeArbiterFrameProductionTest, NopAnimation) {
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // Switching to no-op animation requires 10 consecutive no-op frames.
  for (int i = 0; i < kMinFramesSkippedForIdleAnimation; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameSkipped(/*frame_completed=*/true, /*waiting_on_main=*/false);
  }

  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kNopAnimation);

  // A single produced frame doesn't switch us out of no-op.
  voter_.OnFrameProduced(gfx::Rect(640, 640), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kNopAnimation);

  // But a second one does.
  voter_.OnFrameProduced(gfx::Rect(640, 640), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
}

TEST_F(PowerModeArbiterFrameProductionTest,
       NopAnimationPreservesDamageHistory) {
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // Move into kSmallAnimation.
  for (int i = 0; i < kNumDamageAreas - 1; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);

  // Switching to no-op animation requires 10 consecutive no-op frames.
  for (int i = 0; i < kMinFramesSkippedForIdleAnimation; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);
    voter_.OnFrameSkipped(/*frame_completed=*/true, /*waiting_on_main=*/false);
  }

  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kNopAnimation);

  // A single produced frame doesn't switch us out of no-op.
  voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kNopAnimation);

  // But a second one does, and re-uses the existing damage history.
  voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);
}

TEST_F(PowerModeArbiterFrameProductionTest, SingleFrameResetsNopCounter) {
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // A single frame in between some no-op frames resets the no-op frame counter.
  for (int i = 0; i < kMinFramesSkippedForIdleAnimation / 2; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameSkipped(/*frame_completed=*/true, /*waiting_on_main=*/false);
  }
  voter_.OnFrameProduced(gfx::Rect(320, 320), kDeviceScaleFactor);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
  // We still need another 10 skipped frames.
  for (int i = 0; i < kMinFramesSkippedForIdleAnimation; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    // waiting_on_main=true also counts for no-op frames.
    voter_.OnFrameSkipped(/*frame_completed=*/true, /*waiting_on_main=*/true);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kNopAnimation);
}

TEST_F(PowerModeArbiterFrameProductionTest,
       BeginFrameSignalResetsDamageHistory) {
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // Move into kSmallAnimation.
  for (int i = 0; i < kNumDamageAreas - 1; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);

  // Clearing the BeginFrame signal also clears the damage history.
  voter_.OnNeedsBeginFramesChanged(false);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);
  env_.FastForwardBy(PowerModeVoter::kAnimationTimeout);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kIdle);
  voter_.OnNeedsBeginFramesChanged(true);
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);

  // We need another 9 frames to get back into small animation.
  for (int i = 0; i < kNumDamageAreas - 1; i++) {
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kAnimation);
    voter_.OnFrameProduced(gfx::Rect(160, 160), kDeviceScaleFactor);
  }
  EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kSmallAnimation);
}

}  // namespace power_scheduler
