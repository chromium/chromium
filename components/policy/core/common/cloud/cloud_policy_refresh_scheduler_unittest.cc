// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Mock;
using testing::_;

namespace policy {

namespace {

const int64_t kPolicyRefreshRate = 4 * 60 * 60 * 1000;

const int64_t kInitialCacheAgeMinutes = 1;

}  // namespace

class CloudPolicyRefreshSchedulerTest : public testing::Test {
 protected:
  CloudPolicyRefreshSchedulerTest()
      : service_(std::make_unique<MockCloudPolicyService>(&client_, &store_)),
        task_runner_(new base::TestSimpleTaskRunner()) {}

  void SetUp() override {
    client_.SetDMToken("token");

    // Set up the protobuf timestamp to be one minute in the past. Since the
    // protobuf field only has millisecond precision, we convert the actual
    // value back to get a millisecond-clamped time stamp for the checks below.
    store_.policy_.reset(new em::PolicyData());
    base::Time now = base::Time::NowFromSystemTime();
    base::TimeDelta initial_age =
        base::TimeDelta::FromMinutes(kInitialCacheAgeMinutes);
    store_.policy_->set_timestamp((now - initial_age).ToJavaTime());
    last_update_ = base::Time::FromJavaTime(store_.policy_->timestamp());
    last_update_ticks_ = base::TimeTicks::Now() +
                         (last_update_ - base::Time::NowFromSystemTime());
  }

  CloudPolicyRefreshScheduler* CreateRefreshScheduler() {
    EXPECT_EQ(0u, task_runner_->NumPendingTasks());
    CloudPolicyRefreshScheduler* scheduler = new CloudPolicyRefreshScheduler(
        &client_, &store_, service_.get(), task_runner_,
        network::TestNetworkConnectionTracker::CreateGetter());
    // Make sure the NetworkConnectionTracker has been set up.
    base::RunLoop().RunUntilIdle();
    scheduler->SetDesiredRefreshDelay(kPolicyRefreshRate);
    return scheduler;
  }

  void NotifyConnectionChanged() {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);
    base::RunLoop().RunUntilIdle();
  }

  void EmulateSleepThroughLastRefreshTime(
      CloudPolicyRefreshScheduler* const scheduler) {
    // Simulate a sleep of the device by decreasing the wall clock based refresh
    // timestamp, so that the next refresh time point, calculated from it, turns
    // out to be earlier than the next refresh time point, calculated from the
    // ticks count clock.
    scheduler->set_last_refresh_for_testing(base::Time::NowFromSystemTime() -
                                            base::TimeDelta::FromDays(1));
  }

  base::TimeDelta GetLastDelay() const {
    if (!task_runner_->HasPendingTask())
      return base::TimeDelta();
    return task_runner_->FinalPendingTaskDelay();
  }

  void CheckTiming(int64_t expected_delay_ms) const {
    CheckTimingWithAge(base::TimeDelta::FromMilliseconds(expected_delay_ms),
                       base::TimeDelta());
  }

  // Checks that the latest refresh scheduled used an offset of
  // |offset_from_last_refresh| from the time of the previous refresh.
  // |cache_age| is how old the cache was when the refresh was issued.
  void CheckTimingWithAge(const base::TimeDelta& offset_from_last_refresh,
                          const base::TimeDelta& cache_age) const {
    EXPECT_TRUE(task_runner_->HasPendingTask());
    base::Time now(base::Time::NowFromSystemTime());
    base::TimeTicks now_ticks(base::TimeTicks::Now());
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
    // |offset_from_last_refresh| - elapsed.
    //
    // The delay is also less than offset_from_last_refresh, because some time
    // already elapsed. Additionally, if the cache was already considered old
    // when the schedule was performed then its age at that time has been
    // discounted from the delay. So the delay is a bit less than
    // |offset_from_last_refresh - cache_age|.
    // The logic of time based on TimeTicks is added to be on the safe side,
    // since CloudPolicyRefreshScheduler implementation is based on both, the
    // system time and the time in TimeTicks.
    base::TimeDelta system_delta = (now - last_update_);
    base::TimeDelta ticks_delta = (now_ticks - last_update_ticks_);
    EXPECT_GE(GetLastDelay(),
              offset_from_last_refresh - std::max(system_delta, ticks_delta));
    EXPECT_LE(GetLastDelay(), offset_from_last_refresh - cache_age);
  }

  void CheckInitialRefresh(bool with_invalidations) const {
#if defined(OS_ANDROID) || defined(OS_IOS)
    // The mobile platforms take the cache age into account for the initial
    // fetch. Usually the cache age is ignored for the initial refresh, but on
    // mobile it's used to restrain from refreshing on every startup.
    base::TimeDelta rate = base::TimeDelta::FromMilliseconds(
        with_invalidations
            ? CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs
            : kPolicyRefreshRate);
    CheckTimingWithAge(rate,
                       base::TimeDelta::FromMinutes(kInitialCacheAgeMinutes));
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
  std::unique_ptr<MockCloudPolicyService> service_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // Base time for the refresh that the scheduler should be using.
  base::Time last_update_;
  base::TimeTicks last_update_ticks_;
};

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshNoPolicy) {
  store_.policy_.reset();
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshUnmanaged) {
  store_.policy_->set_state(em::PolicyData::UNMANAGED);
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());
  CheckTiming(CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedNotYetFetched) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  CheckInitialRefresh(false);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedAlreadyFetched) {
  SetLastUpdateToNow();
  client_.SetPolicy(dm_protocol::kChromeUserPolicyType, std::string(),
                    em::PolicyFetchResponse());
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());
  CheckTiming(kPolicyRefreshRate);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, Unregistered) {
  client_.SetDMToken(std::string());
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());
  client_.NotifyPolicyFetched();
  client_.NotifyRegistrationStateChanged();
  client_.NotifyClientError();
  scheduler->SetDesiredRefreshDelay(12 * 60 * 60 * 1000);
  store_.NotifyStoreLoaded();
  store_.NotifyStoreError();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(CloudPolicyRefreshSchedulerTest, RefreshSoon) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  scheduler->RefreshSoon();
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(CloudPolicyRefreshSchedulerTest, RefreshSoonOverriding) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());

  // The refresh scheduled for soon overrides the previously scheduled refresh.
  scheduler->RefreshSoon();
  CheckTiming(0);

  // The refresh scheduled for soon is not overridden by the change of the
  // desired refresh delay.
  const int64_t kNewPolicyRefreshRate = 12 * 60 * 60 * 1000;
  scheduler->SetDesiredRefreshDelay(kNewPolicyRefreshRate);
  CheckTiming(0);

  // The refresh scheduled for soon is not overridden by the notification on the
  // already fetched policy.
  client_.SetPolicy(dm_protocol::kChromeUserPolicyType, std::string(),
                    em::PolicyFetchResponse());
  store_.NotifyStoreLoaded();
  CheckTiming(0);

  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);

  // The next refresh is scheduled according to the normal rate.
  client_.NotifyPolicyFetched();
  CheckTiming(kNewPolicyRefreshRate);
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

  CheckInitialRefresh(true);

  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // Complete that fetch.
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
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
  CheckInitialRefresh(false);
  EXPECT_EQ(kPolicyRefreshRate, scheduler->GetActualRefreshDelay());

  // Perform that fetch now.
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // Complete that fetch.
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled at the normal rate.
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsOffAndOn) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(
          &client_, &store_, service_.get(), task_runner_,
          network::TestNetworkConnectionTracker::CreateGetter()));
  scheduler->SetDesiredRefreshDelay(kPolicyRefreshRate);
  scheduler->SetInvalidationServiceAvailability(true);
  // Initial fetch.
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);

  // If the service goes down and comes back up before the timeout then a
  // refresh is rescheduled at the lower rate again; after executing all
  // pending tasks only 1 fetch is performed.
  scheduler->SetInvalidationServiceAvailability(false);
  scheduler->SetInvalidationServiceAvailability(true);
  // The next refresh has been scheduled using a lower refresh rate.
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
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
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
  SetLastUpdateToNow();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  // Flush that task.
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
  EXPECT_CALL(*service_.get(), RefreshPolicy(_)).Times(1);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // If the service goes down then the refresh scheduler falls back on the
  // default polling rate.
  scheduler->SetInvalidationServiceAvailability(false);
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerTest, OnConnectionChangedUnregistered) {
  client_.SetDMToken(std::string());
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());

  client_.NotifyClientError();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  EmulateSleepThroughLastRefreshTime(scheduler.get());
  scheduler->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

// TODO(igorcov): Before sleep in normal flow there's a task pending. When the
// device wakes up, OnConnectionChanged is called which should cancel the
// pending task and queue a new task to run earlier. It is desirable to
// simulate that flow here.
TEST_F(CloudPolicyRefreshSchedulerTest, OnConnectionChangedAfterSleep) {
  std::unique_ptr<CloudPolicyRefreshScheduler> scheduler(
      CreateRefreshScheduler());

  client_.SetPolicy(dm_protocol::kChromeUserPolicyType, std::string(),
                    em::PolicyFetchResponse());
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  EmulateSleepThroughLastRefreshTime(scheduler.get());
  scheduler->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->ClearPendingTasks();
}

class CloudPolicyRefreshSchedulerSteadyStateTest
    : public CloudPolicyRefreshSchedulerTest {
 protected:
  CloudPolicyRefreshSchedulerSteadyStateTest() {}

  void SetUp() override {
    refresh_scheduler_.reset(CreateRefreshScheduler());
    refresh_scheduler_->SetDesiredRefreshDelay(kPolicyRefreshRate);
    CloudPolicyRefreshSchedulerTest::SetUp();
    SetLastUpdateToNow();
    client_.NotifyPolicyFetched();
    CheckTiming(kPolicyRefreshRate);
  }

  std::unique_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;
};

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnPolicyFetched) {
  client_.NotifyPolicyFetched();
  CheckTiming(kPolicyRefreshRate);
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
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnStoreError) {
  task_runner_->ClearPendingTasks();
  store_.NotifyStoreError();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, RefreshDelayChange) {
  const int delay_short_ms = 5 * 60 * 1000;
  refresh_scheduler_->SetDesiredRefreshDelay(delay_short_ms);
  CheckTiming(CloudPolicyRefreshScheduler::kRefreshDelayMinMs);

  const int delay_ms = 12 * 60 * 60 * 1000;
  refresh_scheduler_->SetDesiredRefreshDelay(delay_ms);
  CheckTiming(delay_ms);

  const int delay_long_ms = 20 * 24 * 60 * 60 * 1000;
  refresh_scheduler_->SetDesiredRefreshDelay(delay_long_ms);
  CheckTiming(CloudPolicyRefreshScheduler::kRefreshDelayMaxMs);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnConnectionChanged) {
  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  NotifyConnectionChanged();
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
}

struct ClientErrorTestParam {
  DeviceManagementStatus client_error;
  int64_t expected_delay_ms;
  int backoff_factor;
};

static const ClientErrorTestParam kClientErrorTestCases[] = {
    {DM_STATUS_REQUEST_INVALID,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_REQUEST_FAILED,
     CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2},
    {DM_STATUS_TEMPORARY_UNAVAILABLE,
     CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2},
    {DM_STATUS_HTTP_STATUS_ERROR,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_RESPONSE_DECODING_ERROR,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
     CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1},
    {DM_STATUS_SERVICE_DEVICE_NOT_FOUND, -1, 1},
    {DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID, -1, 1},
    {DM_STATUS_SERVICE_ACTIVATION_PENDING, kPolicyRefreshRate, 1},
    {DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER, -1, 1},
    {DM_STATUS_SERVICE_MISSING_LICENSES, -1, 1},
    {DM_STATUS_SERVICE_DEVICE_ID_CONFLICT, -1, 1},
    {DM_STATUS_SERVICE_POLICY_NOT_FOUND, kPolicyRefreshRate, 1},
    {DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE, -1, 1},
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
    CheckTiming(expected_delay_ms);

    // Check whether exponential backoff is working as expected and capped at
    // the regular refresh rate (if applicable).
    do {
      expected_delay_ms *= GetParam().backoff_factor;
      SetLastUpdateToNow();
      client_.NotifyClientError();
      CheckTiming(std::max(std::min(expected_delay_ms, kPolicyRefreshRate),
                           GetParam().expected_delay_ms));
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
