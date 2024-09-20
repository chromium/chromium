// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler_observer.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "mock_user_cloud_policy_store.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;
using testing::Invoke;
using testing::Mock;

namespace policy {

namespace {

const int64_t kPolicyRefreshRate = 4 * 60 * 60 * 1000;

const int64_t kInitialCacheAgeMinutes = 1;

constexpr auto kTestReason = PolicyFetchReason::kTest;

class MockObserver : public CloudPolicyRefreshSchedulerObserver {
 public:
  MOCK_METHOD(void,
              OnFetchAttempt,
              (CloudPolicyRefreshScheduler * scheduler),
              (override));
  MOCK_METHOD(void,
              OnRefreshSchedulerDestruction,
              (CloudPolicyRefreshScheduler * scheduler),
              (override));
};

}  // namespace

class CloudPolicyRefreshSchedulerTest : public testing::Test {
 protected:
  CloudPolicyRefreshSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        service_(std::make_unique<MockCloudPolicyService>(&client_, &store_)),
        task_runner_(new base::TestSimpleTaskRunner()),
        mock_clock_(std::make_unique<base::SimpleTestClock>()) {}

  void SetUp() override {
    client_.SetDMToken("token");

    // Set up the protobuf timestamp to be one minute in the past. Since the
    // protobuf field only has millisecond precision, we convert the actual
    // value back to get a millisecond-clamped time stamp for the checks below.
    base::Time now = base::Time::NowFromSystemTime();
    base::TimeDelta initial_age = base::Minutes(kInitialCacheAgeMinutes);
    policy_data_.set_timestamp(
        (now - initial_age).InMillisecondsSinceUnixEpoch());
    store_.set_policy_data_for_testing(
        std::make_unique<em::PolicyData>(policy_data_));
    last_update_ = base::Time::FromMillisecondsSinceUnixEpoch(
        store_.policy()->timestamp());
    last_update_ticks_ = base::TimeTicks::Now() +
                         (last_update_ - base::Time::NowFromSystemTime());

    // Remove mock observer from any scheduler that is being destroyed.
    ON_CALL(mock_observer_, OnRefreshSchedulerDestruction)
        .WillByDefault(Invoke([&](CloudPolicyRefreshScheduler* scheduler) {
          scheduler->RemoveObserver(&mock_observer_);
        }));
  }

  CloudPolicyRefreshScheduler* CreateRefreshScheduler() {
    EXPECT_EQ(0u, task_runner_->NumPendingTasks());
    CloudPolicyRefreshScheduler* scheduler = new CloudPolicyRefreshScheduler(
        &client_, &store_, service_.get(), task_runner_,
        network::TestNetworkConnectionTracker::CreateGetter());
    // Make sure the NetworkConnectionTracker has been set up.
    base::RunLoop().RunUntilIdle();
    scheduler->SetDesiredRefreshDelay(kPolicyRefreshRate);
    scheduler->AddObserver(&mock_observer_);
    return scheduler;
  }

  void NotifyConnectionChanged() {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);
    base::RunLoop().RunUntilIdle();
  }

  base::ScopedClosureRunner EmulateSleepThroughLastRefreshTime() {
    // Mock wall clock time, but make sure that it starts from current time.
    mock_clock_->Advance(base::Time::Now() - base::Time());

    // Simulate a sleep of the device by advancing the wall clock time, but
    // keeping tick count clock unchanged. Since tick clock is not mocked, some
    // time will pass and difference between clocks will be smaller than 1 day,
    // but it should still be larger than the refresh rate (4 hours).
    mock_clock_->Advance(base::Days(1));

    return CloudPolicyRefreshScheduler::OverrideClockForTesting(
        mock_clock_.get());
  }

  base::TimeDelta GetLastDelay() const {
    if (!task_runner_->HasPendingTask())
      return base::TimeDelta();
    return task_runner_->FinalPendingTaskDelay();
  }

  void CheckTiming(CloudPolicyRefreshScheduler* const scheduler,
                   int64_t expected_delay_ms) const {
    CheckTimingWithAge(scheduler, base::Milliseconds(expected_delay_ms),
                       base::TimeDelta());
  }

  // Checks that the latest refresh scheduled used an offset of
  // |offset_from_last_refresh| from the time of the previous refresh.
  // |cache_age| is how old the cache was when the refresh was issued.
  void CheckTimingWithAge(CloudPolicyRefreshScheduler* const scheduler,
                          const base::TimeDelta& offset_from_last_refresh,
                          const base::TimeDelta& cache_age) const {
    EXPECT_TRUE(task_runner_->HasPendingTask());
    base::Time now(base::Time::NowFromSystemTime());
    base::TimeTicks now_ticks(base::TimeTicks::Now());
    base::TimeDelta offset_since_refresh_plus_salt = offset_from_last_refresh;
    // The salt is only applied for non-immediate scheduled refreshes.
    if (!offset_from_last_refresh.is_zero()) {
      offset_since_refresh_plus_salt +=
          base::Milliseconds(scheduler->GetSaltDelayForTesting());
    }
    // |last_update_| was updated and then a refresh was scheduled at time S,
    // so |last_update_| is a bit before that.
    // Now is a bit later, N.
    // GetLastDelay() + S is the time when the refresh will run, T.
    // |cache_age| is the age of the cache at time S. It was thus created at
    // S - cache_age.
    //
    // Schematically:
    //
    // . S . N . . . . . . . T . . . .
    //   |   |               |
    //   set "last_refresh_" and then scheduled the next refresh; the cache
    //   was "cache_age" old at this point.
    //       |               |
    //       some time elapsed on the test execution since then;
    //       this is the current time, "now"
    //                       |
    //                       the refresh will execute at this time
    //
    // So the exact delay is T - S - |cache_age|, but we don't have S here.
    //
    // |last_update_| was a bit before S, so if
    // elapsed = now - |last_update_| then the delay is more than
    // |offset_since_refresh_plus_salt| - elapsed.
    //
    // The delay is also less than offset_since_refresh_plus_salt, because some
    // time already elapsed. Additionally, if the cache was already considered
    // old when the schedule was performed then its age at that time has been
    // discounted from the delay. So the delay is a bit less than
    // |offset_since_refresh_plus_salt - cache_age|.
    // The logic of time based on TimeTicks is added to be on the safe side,
    // since CloudPolicyRefreshScheduler implementation is based on both, the
    // system time and the time in TimeTicks.
    base::TimeDelta system_delta = (now - last_update_);
    base::TimeDelta ticks_delta = (now_ticks - last_update_ticks_);
    EXPECT_GE(GetLastDelay(), offset_since_refresh_plus_salt -
                                  std::max(system_delta, ticks_delta));
    EXPECT_LE(GetLastDelay(), offset_since_refresh_plus_salt - cache_age);
  }

  void CheckInitialRefresh(CloudPolicyRefreshScheduler* const scheduler,
                           bool with_invalidations) const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    // The mobile platforms take the cache age into account for the initial
    // fetch. Usually the cache age is ignored for the initial refresh, but on
    // mobile it's used to restrain from refreshing on every startup.
    base::TimeDelta rate = base::Milliseconds(
        with_invalidations
            ? CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs
            : kPolicyRefreshRate);
    CheckTimingWithAge(scheduler, rate, base::Minutes(kInitialCacheAgeMinutes));
#else
    // Other platforms refresh immediately.
    EXPECT_EQ(base::TimeDelta(), GetLastDelay());
#endif
  }

  void SetLastUpdateToNow() {
    last_update_ = base::Time::NowFromSystemTime();
    last_update_ticks_ = base::TimeTicks::Now();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockCloudPolicyClient client_;
  MockCloudPolicyStore store_;
  em::PolicyData policy_data_;
  std::unique_ptr<MockCloudPolicyService> service_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  MockObserver mock_observer_;

  // Base time for the refresh that the scheduler should be using.
  base::Time last_update_;
  base::TimeTicks last_update_ticks_;

  std::unique_ptr<base::SimpleTestClock> mock_clock_;
};

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshNoPolicy) {
  store_.set_policy_data_for_testing(std::make_unique<em::PolicyData>());
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshUnmanaged) {
  policy_data_.set_state(em::PolicyData::UNMANAGED);
  store_.set_policy_data_for_testing(
      std::make_unique<em::PolicyData>(policy_data_));
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  CheckTiming(scheduler.get(),
              CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedNotYetFetched) {
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  CheckInitialRefresh(scheduler.get(), false);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedAlreadyFetched) {
  SetLastUpdateToNow();
  client_.SetPolicy(dm_protocol::kChromeUserPolicyType, std::string(),
                    em::PolicyFetchResponse());
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  CheckTiming(scheduler.get(), kPolicyRefreshRate);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, Unregistered) {
  client_.SetDMToken(std::string());
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  client_.NotifyPolicyFetched();
  client_.NotifyRegistrationStateChanged();
  client_.NotifyClientError();
  scheduler->SetDesiredRefreshDelay(12 * 60 * 60 * 1000);
  store_.NotifyStoreLoaded();
  store_.NotifyStoreError();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(CloudPolicyRefreshSchedulerTest, RefreshSoon) {
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, kTestReason)).Times(1);
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  scheduler->RefreshSoon(kTestReason);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(CloudPolicyRefreshSchedulerTest, RefreshSoonOverriding) {
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());

  // The refresh scheduled for soon overrides the previously scheduled refresh.
  scheduler->RefreshSoon(kTestReason);
  CheckTiming(scheduler.get(), 0);

  // The refresh scheduled for soon is not overridden by the change of the
  // desired refresh delay.
  const int64_t kNewPolicyRefreshRate = 12 * 60 * 60 * 1000;
  scheduler->SetDesiredRefreshDelay(kNewPolicyRefreshRate);
  CheckTiming(scheduler.get(), 0);

  // The refresh scheduled for soon is not overridden by the notification on the
  // already fetched policy.
  client_.SetPolicy(dm_protocol::kChromeUserPolicyType, std::string(),
                    em::PolicyFetchResponse());
  store_.NotifyStoreLoaded();
  CheckTiming(scheduler.get(), 0);

  EXPECT_CALL(*service_.get(), RefreshPolicy(_, kTestReason)).Times(1);
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);

  // The next refresh is scheduled according to the normal rate.
  client_.NotifyPolicyFetched();
  CheckTiming(scheduler.get(), kNewPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsAvailable) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(
          &client_, &store_, service_.get(), task_runner_,
          network::TestNetworkConnectionTracker::CreateGetter()));
  scheduler->SetDesiredRefreshDelay(kPolicyRefreshRate);

  // The scheduler has scheduled refreshes at the initial refresh rate.
  EXPECT_EQ(2u, task_runner_->NumPendingTasks());

  // Signal that invalidations are available.
  scheduler->SetInvalidationServiceAvailability(true);
  EXPECT_EQ(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs,
            scheduler->GetActualRefreshDelay());
  EXPECT_EQ(3u, task_runner_->NumPendingTasks());

  CheckInitialRefresh(scheduler.get(), true);

  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // Complete that fetch.
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  CheckTiming(scheduler.get(),
              CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsNotAvailable) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(
          &client_, &store_, service_.get(), task_runner_,
          network::TestNetworkConnectionTracker::CreateGetter()));
  scheduler->SetDesiredRefreshDelay(kPolicyRefreshRate);

  // Signal that invalidations are not available. The scheduler will not
  // schedule refreshes since the available state is not changed.
  for (int i = 0; i < 10; ++i) {
    scheduler->SetInvalidationServiceAvailability(false);
    EXPECT_EQ(2u, task_runner_->NumPendingTasks());
  }

  // This scheduled the initial refresh.
  CheckInitialRefresh(scheduler.get(), false);
  EXPECT_EQ(kPolicyRefreshRate, scheduler->GetActualRefreshDelay());

  // Perform that fetch now.
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // Complete that fetch.
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled at the normal rate.
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  CheckTiming(scheduler.get(), kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsOffAndOn) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(
          &client_, &store_, service_.get(), task_runner_,
          network::TestNetworkConnectionTracker::CreateGetter()));
  scheduler->SetDesiredRefreshDelay(kPolicyRefreshRate);
  scheduler->SetInvalidationServiceAvailability(true);
  // Initial fetch.
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  CheckTiming(scheduler.get(),
              CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);

  // If the service goes down and comes back up before the timeout then a
  // refresh is rescheduled at the lower rate again; after executing all
  // pending tasks only 1 fetch is performed.
  scheduler->SetInvalidationServiceAvailability(false);
  scheduler->SetInvalidationServiceAvailability(true);
  // The next refresh has been scheduled using a lower refresh rate.
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  CheckTiming(scheduler.get(),
              CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsDisconnected) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(
          &client_, &store_, service_.get(), task_runner_,
          network::TestNetworkConnectionTracker::CreateGetter()));
  scheduler->SetDesiredRefreshDelay(kPolicyRefreshRate);
  scheduler->SetInvalidationServiceAvailability(true);
  // Initial fetch.
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  // Flush that task.
  CheckTiming(scheduler.get(),
              CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_, PolicyFetchReason::kScheduled))
      .Times(1);
  EXPECT_CALL(client_, FetchPolicy(PolicyFetchReason::kScheduled)).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // If the service goes down then the refresh scheduler falls back on the
  // default polling rate.
  scheduler->SetInvalidationServiceAvailability(false);
  CheckTiming(scheduler.get(), kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerTest, OnConnectionChangedUnregistered) {
  client_.SetDMToken(std::string());
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());

  client_.NotifyClientError();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  auto closure = EmulateSleepThroughLastRefreshTime();
  scheduler->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

// TODO(igorcov): Before sleep in normal flow there's a task pending. When the
// device wakes up, OnConnectionChanged is called which should cancel the
// pending task and queue a new task to run earlier. It is desirable to
// simulate that flow here.
TEST_F(CloudPolicyRefreshSchedulerTest, OnConnectionChangedAfterSleep) {
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());

  client_.SetPolicy(dm_protocol::kChromeUserPolicyType, std::string(),
                    em::PolicyFetchResponse());
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  auto closure = EmulateSleepThroughLastRefreshTime();
  scheduler->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->ClearPendingTasks();
}

TEST_F(CloudPolicyRefreshSchedulerTest, FetchAttemptCallback) {
  EXPECT_CALL(mock_observer_, OnFetchAttempt).Times(0);
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnFetchAttempt).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_observer_);
}

TEST_F(CloudPolicyRefreshSchedulerTest, DestructionCallback) {
  EXPECT_CALL(mock_observer_, OnRefreshSchedulerDestruction).Times(0);
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnRefreshSchedulerDestruction).Times(1);
  scheduler.reset();
  Mock::VerifyAndClearExpectations(&mock_observer_);
}

TEST_F(CloudPolicyRefreshSchedulerTest, DestructionCallbackBeforeFetchAttempt) {
  EXPECT_CALL(mock_observer_, OnRefreshSchedulerDestruction).Times(0);
  EXPECT_CALL(mock_observer_, OnFetchAttempt).Times(0);
  auto scheduler = base::WrapUnique(CreateRefreshScheduler());
  Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnRefreshSchedulerDestruction(scheduler.get()))
      .Times(1);
  scheduler.reset();
  Mock::VerifyAndClearExpectations(&mock_observer_);
}

class CloudPolicyRefreshSchedulerSteadyStateTest
    : public CloudPolicyRefreshSchedulerTest {
 protected:
  CloudPolicyRefreshSchedulerSteadyStateTest() = default;

  void SetUp() override {
    refresh_scheduler_.reset(CreateRefreshScheduler());
    refresh_scheduler_->SetDesiredRefreshDelay(kPolicyRefreshRate);
    CloudPolicyRefreshSchedulerTest::SetUp();
    SetLastUpdateToNow();
    client_.NotifyPolicyFetched();
    CheckTiming(refresh_scheduler_.get(), kPolicyRefreshRate);
  }

  std::unique_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;
};

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnPolicyFetched) {
  client_.NotifyPolicyFetched();
  CheckTiming(refresh_scheduler_.get(), kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnRegistrationStateChanged) {
  client_.SetDMToken("new_token");
  client_.NotifyRegistrationStateChanged();
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());

  task_runner_->ClearPendingTasks();
  client_.SetDMToken(std::string());
  client_.NotifyRegistrationStateChanged();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnStoreLoaded) {
  store_.NotifyStoreLoaded();
  CheckTiming(refresh_scheduler_.get(), kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnStoreError) {
  task_runner_->ClearPendingTasks();
  store_.NotifyStoreError();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, RefreshDelayChange) {
  const int delay_short_ms = 5 * 60 * 1000;
  refresh_scheduler_->SetDesiredRefreshDelay(delay_short_ms);
  CheckTiming(refresh_scheduler_.get(),
              CloudPolicyRefreshScheduler::kRefreshDelayMinMs);

  const int delay_ms = 12 * 60 * 60 * 1000;
  refresh_scheduler_->SetDesiredRefreshDelay(delay_ms);
  CheckTiming(refresh_scheduler_.get(), delay_ms);

  const int delay_long_ms = 20 * 24 * 60 * 60 * 1000;
  refresh_scheduler_->SetDesiredRefreshDelay(delay_long_ms);
  CheckTiming(refresh_scheduler_.get(),
              CloudPolicyRefreshScheduler::kRefreshDelayMaxMs);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnConnectionChanged) {
  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  NotifyConnectionChanged();
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest,
       SignatureValidationFailedAndRetry) {
  MockUserCloudPolicyStore store;
  refresh_scheduler_ = std::make_unique<CloudPolicyRefreshScheduler>(
      &client_, &store, service_.get(), task_runner_,
      network::TestNetworkConnectionTracker::CreateGetter());

  task_runner_->ClearPendingTasks();

  client_.SetDMToken("dm-token");

  // In case of signature error, reset key and retry.
  EXPECT_CALL(store, ResetPolicyKey()).Times(1);
  store.validation_result_ =
      std::make_unique<CloudPolicyValidatorBase::ValidationResult>();
  store.validation_result_->status =
      CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE;
  store.status_ = CloudPolicyStore::STATUS_VALIDATION_ERROR;

  store.NotifyStoreError();

  EXPECT_TRUE(task_runner_->HasPendingTask());

  // If it happens twice in row, won't retry for the second time.
  Mock::VerifyAndClearExpectations(&store);
  task_runner_->ClearPendingTasks();
  EXPECT_CALL(store, ResetPolicyKey()).Times(0);

  store.NotifyStoreError();

  EXPECT_FALSE(task_runner_->HasPendingTask());

  refresh_scheduler_.reset();
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

struct ClientErrorTestParam {
  DeviceManagementStatus client_error;
  PolicyFetchReason reason;
  int64_t expected_delay_ms;
  int backoff_factor;
};

constexpr PolicyFetchReason kUnspecified = PolicyFetchReason::kUnspecified;

static const ClientErrorTestParam kClientErrorTestCases[] = {
    {DM_STATUS_REQUEST_INVALID,
     PolicyFetchReason::kRetryAfterStatusRequestInvalid,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_REQUEST_FAILED,
     PolicyFetchReason::kRetryAfterStatusRequestFailed,
     CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2},
    {DM_STATUS_TEMPORARY_UNAVAILABLE,
     PolicyFetchReason::kRetryAfterStatusTemporaryUnavailable,
     CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2},
    {DM_STATUS_HTTP_STATUS_ERROR,
     PolicyFetchReason::kRetryAfterStatusHttpStatusError,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_RESPONSE_DECODING_ERROR,
     PolicyFetchReason::kRetryAfterStatusResponseDecodingError,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
     PolicyFetchReason::kRetryAfterStatusServiceManagementNotSupported,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_REQUEST_TOO_LARGE,
     PolicyFetchReason::kRetryAfterStatusRequestTooLarge,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_SERVICE_DEVICE_NOT_FOUND, kUnspecified, -1, 1},
    {DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID, kUnspecified, -1, 1},
    {DM_STATUS_SERVICE_ACTIVATION_PENDING,
     PolicyFetchReason::kRetryAfterStatusServiceActivationPending,
     kPolicyRefreshRate, 1},
    {DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER, kUnspecified, -1, 1},
    {DM_STATUS_SERVICE_MISSING_LICENSES, kUnspecified, -1, 1},
    {DM_STATUS_SERVICE_DEVICE_ID_CONFLICT, kUnspecified, -1, 1},
    {DM_STATUS_SERVICE_POLICY_NOT_FOUND,
     PolicyFetchReason::kRetryAfterStatusServicePolicyNotFound,
     kPolicyRefreshRate, 1},
    {DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE, kUnspecified, -1,
     1},
    {DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL,
     kUnspecified, -1, 1},
    {DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED, kUnspecified, -1,
     1},
    {DM_STATUS_SERVICE_TOO_MANY_REQUESTS,
     PolicyFetchReason::kRetryAfterStatusServiceTooManyRequests,
     kPolicyRefreshRate, 1},
    {DM_STATUS_SERVICE_DEVICE_NEEDS_RESET, kUnspecified, -1, 1},
    {DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE, kUnspecified,
     -1, 1},
    {DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK, kUnspecified, -1, 1},
};

class CloudPolicyRefreshSchedulerClientErrorTest
    : public CloudPolicyRefreshSchedulerSteadyStateTest,
      public testing::WithParamInterface<ClientErrorTestParam> {};

TEST_P(CloudPolicyRefreshSchedulerClientErrorTest, OnClientError) {
  client_.SetStatus(GetParam().client_error);
  task_runner_->ClearPendingTasks();

  // See whether the error triggers the right refresh delay.
  int64_t expected_delay_ms = GetParam().expected_delay_ms;
  client_.NotifyClientError();
  if (expected_delay_ms >= 0) {
    CheckTiming(refresh_scheduler_.get(), expected_delay_ms);

    // Check whether exponential backoff is working as expected and capped at
    // the regular refresh rate (if applicable).
    do {
      EXPECT_CALL(mock_observer_, OnFetchAttempt).Times(testing::AtLeast(1));
      expected_delay_ms *= GetParam().backoff_factor;
      SetLastUpdateToNow();
      client_.NotifyClientError();
      CheckTiming(refresh_scheduler_.get(),
                  std::max(std::min(expected_delay_ms, kPolicyRefreshRate),
                           GetParam().expected_delay_ms));
      mock_clock_->Advance(GetLastDelay());
      EXPECT_CALL(*service_.get(), RefreshPolicy(_, GetParam().reason))
          .Times(1);
      task_runner_->RunUntilIdle();
    } while (GetParam().backoff_factor > 1 &&
             expected_delay_ms <= kPolicyRefreshRate);
  } else {
    EXPECT_EQ(base::TimeDelta(), GetLastDelay());
    EXPECT_FALSE(task_runner_->HasPendingTask());
  }
}

INSTANTIATE_TEST_SUITE_P(CloudPolicyRefreshSchedulerClientErrorTest,
                         CloudPolicyRefreshSchedulerClientErrorTest,
                         testing::ValuesIn(kClientErrorTestCases));

}  // namespace policy
