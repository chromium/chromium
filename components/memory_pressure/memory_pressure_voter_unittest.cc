// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_voter.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

class TestDelegate
    : public memory_pressure::MemoryPressureVoteAggregator::Delegate {
 public:
  MOCK_METHOD(void,
              OnMemoryPressureLevelChanged,
              (base::MemoryPressureLevel level, bool is_disk_only_critical),
              (override));
  MOCK_METHOD(void,
              OnDiskOnlyCriticalChanged,
              (bool is_disk_only_critical),
              (override));
  MOCK_METHOD(void, OnNotifyListenersRequested, (), (override));
};

}  // namespace

TEST(MemoryPressureVoterTest, EvaluateVotes) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);

  aggregator.SetVotesForTesting(1, 2, 3);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.SetVotesForTesting(1, 20, 1);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.SetVotesForTesting(0, 0, 0);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_NONE);

  aggregator.SetVotesForTesting(0, 2, 0);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Reset votes so destructor doesn't think there are loose voters.
  aggregator.SetVotesForTesting(0, 0, 0);
}

TEST(MemoryPressureVoterTest, OnVote) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);

  // vote count = 0,0,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_NONE);

  aggregator.OnVoteForTesting(std::nullopt, base::MEMORY_PRESSURE_LEVEL_NONE);
  // vote count = 1,0,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_NONE);

  aggregator.OnVoteForTesting(std::nullopt,
                              base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // vote count = 1,0,1
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.OnVoteForTesting(std::nullopt,
                              base::MEMORY_PRESSURE_LEVEL_MODERATE);
  // vote count = 1,1,1
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.OnVoteForTesting(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                              base::MEMORY_PRESSURE_LEVEL_MODERATE);
  // vote count = 1,2,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_MODERATE);

  aggregator.OnVoteForTesting(base::MEMORY_PRESSURE_LEVEL_MODERATE,
                              std::nullopt);
  // vote count = 1,1,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Reset votes so destructor doesn't think there are loose voters.
  aggregator.SetVotesForTesting(0, 0, 0);
}

TEST(MemoryPressureVoterTest, SetVote) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto voter_critical = aggregator.CreateVoter();
  auto voter_moderate = aggregator.CreateVoter();

  voter_critical->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  voter_moderate->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  voter_critical.reset();
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_MODERATE);

  voter_moderate.reset();
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_NONE);
}

TEST(MemoryPressureVoterTest, NotificationsAndCooldownTimer) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto voter = aggregator.CreateVoter();

  // Initial NONE vote does not notify listeners.
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE);

  // Transition to MODERATE notifies immediately and starts the 10s timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_MODERATE, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Repeated MODERATE votes before the cooldown do not trigger notifications or
  // reset the timer.
  task_environment.FastForwardBy(base::Seconds(2));
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment.FastForwardBy(base::Seconds(2));
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // At 10s total, the moderate repeat timer fires.
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kModeratePressureCooldown -
      base::Seconds(4));
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Transition to CRITICAL notifies immediately and switches to the 5s timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_CRITICAL, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kCriticalPressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Transition to NONE notifies immediately and stops the timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_NONE, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kModeratePressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST(MemoryPressureVoterTest, MultipleVotersDoNotMultiplyNotifications) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto voter1 = aggregator.CreateVoter();
  auto voter2 = aggregator.CreateVoter();

  // Voter 1 enters CRITICAL: notifies once and starts 5s timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_CRITICAL, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  voter1->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Voter 2 also enters CRITICAL 2s later: aggregate level is unchanged, so no
  // immediate notification and the 5s timer is not reset.
  task_environment.FastForwardBy(base::Seconds(2));
  voter2->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // At t=5s, the timer fires once (not twice).
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kCriticalPressureCooldown -
      base::Seconds(2));
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Voter 2 drops to MODERATE while Voter 1 remains CRITICAL: aggregate stays
  // CRITICAL, so no transition notification is sent.
  voter2->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Cleanup votes before delegate goes out of scope.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_MODERATE, false));
  voter1.reset();
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_NONE, false));
  voter2.reset();
}

TEST(MemoryPressureVoterTest, CallerManagedNotificationsDoNotStartTimer) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto voter = aggregator.CreateVoter();

  // 2-arg SetVote with notify_listeners=false updates the aggregate level
  // without notifying listeners or starting the cooldown timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_CRITICAL, false));
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                 /*notify_listeners=*/false);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kCriticalPressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // 2-arg SetVote with notify_listeners=true notifies immediately, and still
  // does not start the periodic timer.
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                 /*notify_listeners=*/true);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kCriticalPressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_NONE, false));
  voter.reset();
}

TEST(MemoryPressureVoterTest, ExplicitNotifyListenersResetsTimer) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto periodic_voter = aggregator.CreateVoter();
  auto caller_managed_voter = aggregator.CreateVoter();

  // 1-arg SetVote transitions to CRITICAL, notifying immediately and starting
  // the 5s timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_CRITICAL, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  periodic_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // 3s later, a second voter at the same level with notify_listeners=true
  // forces an immediate notification and resets the 5s timer, which keeps
  // running because the first voter still requests periodic re-notification.
  task_environment.FastForwardBy(base::Seconds(3));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  caller_managed_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                                /*notify_listeners=*/true);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // 2s later (5s from original transition, 2s from explicit notify), timer
  // should not fire yet.
  task_environment.FastForwardBy(base::Seconds(2));
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // 3s later (5s from explicit notify), timer fires.
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  task_environment.FastForwardBy(base::Seconds(3));
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Cleanup votes before delegate goes out of scope.
  periodic_voter.reset();
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_NONE, false));
  caller_managed_voter.reset();
}

TEST(MemoryPressureVoterTest, CallerManagedCriticalOverridesPeriodicModerate) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto periodic_voter = aggregator.CreateVoter();
  auto caller_managed_voter = aggregator.CreateVoter();

  // The periodic voter enters MODERATE: notifies once and starts the moderate
  // timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_MODERATE, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  periodic_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kModeratePressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // The caller-managed voter raises the aggregate level to CRITICAL. Since the
  // only vote at CRITICAL does not request periodic re-notification, the timer
  // stops and only the explicit notification is delivered.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_CRITICAL, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  caller_managed_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                                /*notify_listeners=*/true);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  task_environment.FastForwardBy(
      2 * MemoryPressureVoteAggregator::kModeratePressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // The caller-managed voter drops back to NONE: the aggregate level returns
  // to MODERATE, held by the periodic voter, so the moderate timer resumes.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_MODERATE, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  caller_managed_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE,
                                /*notify_listeners=*/true);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kModeratePressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Cleanup votes before delegate goes out of scope.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_NONE, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  periodic_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE);
  testing::Mock::VerifyAndClearExpectations(&delegate);
  periodic_voter.reset();
  caller_managed_voter.reset();
}

TEST(MemoryPressureVoterTest, PeriodicVoteJoiningCallerManagedLevel) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto periodic_voter = aggregator.CreateVoter();
  auto caller_managed_voter = aggregator.CreateVoter();

  // The caller-managed voter enters CRITICAL first: no timer.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_CRITICAL, false));
  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  caller_managed_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                                /*notify_listeners=*/true);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  task_environment.FastForwardBy(
      2 * MemoryPressureVoteAggregator::kCriticalPressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // The periodic voter also enters CRITICAL: the aggregate level is unchanged
  // so there is no immediate notification, but the 5s timer starts.
  periodic_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnNotifyListenersRequested());
  task_environment.FastForwardBy(
      MemoryPressureVoteAggregator::kCriticalPressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // The periodic voter drops to MODERATE while the caller-managed vote holds
  // CRITICAL: the aggregate level is unchanged and the timer stops.
  periodic_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment.FastForwardBy(
      2 * MemoryPressureVoteAggregator::kCriticalPressureCooldown);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Cleanup votes before delegate goes out of scope.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_MODERATE, false));
  caller_managed_voter.reset();
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_NONE, false));
  periodic_voter.reset();
}

TEST(MemoryPressureVoterTest, DiskOnlyCriticalTracking) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<TestDelegate> delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto disk_voter = aggregator.CreateVoter(/*is_disk_voter=*/true);
  auto memory_voter = aggregator.CreateVoter();

  // Disk voter enters CRITICAL alone: the level change carries the disk-only
  // state, with no separate OnDiskOnlyCriticalChanged() call.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_CRITICAL, true));
  disk_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                      /*notify_listeners=*/false);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Memory voter enters MODERATE: critical is still caused solely by disk.
  memory_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE,
                        /*notify_listeners=*/false);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Memory voter enters CRITICAL: critical is no longer disk-only, while the
  // aggregate level stays CRITICAL.
  EXPECT_CALL(delegate, OnDiskOnlyCriticalChanged(false));
  memory_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                        /*notify_listeners=*/false);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Memory voter drops back to MODERATE while disk stays CRITICAL: critical
  // becomes disk-only again.
  EXPECT_CALL(delegate, OnDiskOnlyCriticalChanged(true));
  memory_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE,
                        /*notify_listeners=*/false);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // Disk voter drops to NONE: the level change to MODERATE implies that
  // critical is no longer disk-only.
  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_MODERATE, false));
  disk_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE,
                      /*notify_listeners=*/false);
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnMemoryPressureLevelChanged(
                            base::MEMORY_PRESSURE_LEVEL_NONE, false));
  memory_voter.reset();
  disk_voter.reset();
}

}  // namespace memory_pressure
