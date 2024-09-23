// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestPrefName[] = "test_pref_name";

// Copied from nearby_scheduler_impl.cc.
constexpr base::TimeDelta kZeroTimeDelta = base::Seconds(0);
constexpr base::TimeDelta kBaseRetryDelay = base::Seconds(5);
constexpr base::TimeDelta kMaxRetryDelay = base::Hours(1);

constexpr base::TimeDelta kTestTimeUntilRecurringRequest = base::Minutes(123);

}  // namespace

namespace ash::nearby {

class NearbySchedulerBaseForTest : public NearbySchedulerBase {
 public:
  NearbySchedulerBaseForTest(
      std::optional<base::TimeDelta> time_until_recurring_request,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      OnRequestCallback callback,
      Feature logging_feature,
      const base::Clock* clock)
      : NearbySchedulerBase(retry_failures,
                            require_connectivity,
                            pref_name,
                            pref_service,
                            std::move(callback),
                            logging_feature,
                            clock),
        time_until_recurring_request_(time_until_recurring_request) {}

  ~NearbySchedulerBaseForTest() override = default;

 private:
  std::optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const override {
    return time_until_recurring_request_;
  }

  std::optional<base::TimeDelta> time_until_recurring_request_;
};

class NearbySchedulerBaseTest : public ::testing::Test {
 protected:
  NearbySchedulerBaseTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {}

  ~NearbySchedulerBaseTest() override = default;

  void SetUp() override {
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    pref_service_.registry()->RegisterDictionaryPref(kTestPrefName);
    SetNetworkConnection(/*online=*/true);
  }

  void OnRequestCallback() { ++on_request_call_count_; }

  void CreateScheduler(
      bool retry_failures,
      bool require_connectivity,
      std::optional<base::TimeDelta> time_until_recurring_request =
          kTestTimeUntilRecurringRequest) {
    scheduler_ = std::make_unique<NearbySchedulerBaseForTest>(
        time_until_recurring_request, retry_failures, require_connectivity,
        kTestPrefName, &pref_service_,
        base::BindRepeating(&NearbySchedulerBaseTest::OnRequestCallback,
                            base::Unretained(this)),
        Feature::NS, task_environment_.GetMockClock());
  }

  void DestroyScheduler() { scheduler_.reset(); }

  void StartScheduling() {
    scheduler_->Start();
    EXPECT_TRUE(scheduler_->is_running());
  }

  void StopScheduling() {
    scheduler_->Stop();
    EXPECT_FALSE(scheduler_->is_running());
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void RunPendingRequest() {
    EXPECT_FALSE(scheduler_->IsWaitingForResult());
    std::optional<base::TimeDelta> time_until_next_request =
        scheduler_->GetTimeUntilNextRequest();
    ASSERT_TRUE(time_until_next_request);
    FastForward(*time_until_next_request);
  }

  void FinishPendingRequest(bool success) {
    EXPECT_TRUE(scheduler_->IsWaitingForResult());
    EXPECT_FALSE(scheduler_->GetTimeUntilNextRequest());
    size_t num_failures = scheduler_->GetNumConsecutiveFailures();
    std::optional<base::Time> last_success_time =
        scheduler_->GetLastSuccessTime();
    scheduler_->HandleResult(success);
    EXPECT_FALSE(scheduler_->IsWaitingForResult());
    EXPECT_EQ(success ? 0 : num_failures + 1,
              scheduler_->GetNumConsecutiveFailures());
    EXPECT_EQ(
        success ? std::make_optional<base::Time>(Now()) : last_success_time,
        scheduler_->GetLastSuccessTime());
  }

  void SetNetworkConnection(bool online) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        online ? network::mojom::ConnectionType::CONNECTION_WIFI
               : network::mojom::ConnectionType::CONNECTION_NONE);
  }

  size_t on_request_call_count() const { return on_request_call_count_; }
  NearbyScheduler* scheduler() { return scheduler_.get(); }

 private:
  size_t on_request_call_count_ = 0;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NearbyScheduler> scheduler_;
};

TEST_F(NearbySchedulerBaseTest, ImmediateRequest) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  scheduler()->MakeImmediateRequest();
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, RecurringRequest) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(kTestTimeUntilRecurringRequest,
            scheduler()->GetTimeUntilNextRequest());

  RunPendingRequest();
  FinishPendingRequest(/*success=*/true);
  EXPECT_EQ(kTestTimeUntilRecurringRequest,
            scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbySchedulerBaseTest, NoRecurringRequest) {
  // The flavor of the schedule does not schedule recurring requests.
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true,
                  /*time_until_recurring_request=*/std::nullopt);
  StartScheduling();
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());

  scheduler()->MakeImmediateRequest();
  RunPendingRequest();
  FinishPendingRequest(/*success=*/true);

  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbySchedulerBaseTest, SchedulingNotStarted) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  EXPECT_FALSE(scheduler()->is_running());
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());

  // Request remains pending until scheduling starts.
  scheduler()->MakeImmediateRequest();
  EXPECT_FALSE(scheduler()->is_running());
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  StartScheduling();
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
}

TEST_F(NearbySchedulerBaseTest, DoNotRetryFailures) {
  CreateScheduler(/*retry_failures=*/false, /*require_connectivity=*/true);
  StartScheduling();

  // Run recurring request.
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  FinishPendingRequest(/*success=*/false);
  EXPECT_EQ(1u, scheduler()->GetNumConsecutiveFailures());

  // Failure is not automatically retried; the recurring request is re-scheduled
  // instead.
  EXPECT_EQ(kTestTimeUntilRecurringRequest,
            scheduler()->GetTimeUntilNextRequest());

  RunPendingRequest();
  EXPECT_EQ(2u, on_request_call_count());
  FinishPendingRequest(/*success=*/false);
  EXPECT_EQ(2u, scheduler()->GetNumConsecutiveFailures());
}

TEST_F(NearbySchedulerBaseTest, FailureRetry) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  scheduler()->MakeImmediateRequest();

  size_t num_failures = 0;
  size_t expected_backoff_factor = 1;
  do {
    EXPECT_EQ(num_failures, scheduler()->GetNumConsecutiveFailures());
    RunPendingRequest();
    EXPECT_EQ(num_failures + 1, on_request_call_count());
    FinishPendingRequest(/*success=*/false);
    EXPECT_EQ(
        std::min(kMaxRetryDelay, kBaseRetryDelay * expected_backoff_factor),
        scheduler()->GetTimeUntilNextRequest());
    expected_backoff_factor *= 2;
    ++num_failures;
  } while (*scheduler()->GetTimeUntilNextRequest() != kMaxRetryDelay);

  RunPendingRequest();
  EXPECT_EQ(num_failures + 1, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, FailureRetry_InterruptWithImmediateAttempt) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  scheduler()->MakeImmediateRequest();

  size_t num_failures = 0;
  size_t expected_backoff_factor = 1;
  do {
    EXPECT_EQ(num_failures, scheduler()->GetNumConsecutiveFailures());
    RunPendingRequest();
    EXPECT_EQ(num_failures + 1, on_request_call_count());
    FinishPendingRequest(/*success=*/false);
    EXPECT_EQ(
        std::min(kMaxRetryDelay, kBaseRetryDelay * expected_backoff_factor),
        scheduler()->GetTimeUntilNextRequest());
    expected_backoff_factor *= 2;
    ++num_failures;
  } while (num_failures < 3);

  // Interrupt retry schedule with immediate request. On failure, it continues
  // the retry strategy using the next backoff.
  EXPECT_EQ(num_failures, scheduler()->GetNumConsecutiveFailures());
  scheduler()->MakeImmediateRequest();
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());
  RunPendingRequest();
  EXPECT_EQ(num_failures + 1, on_request_call_count());
  FinishPendingRequest(/*success=*/false);
  EXPECT_EQ(std::min(kMaxRetryDelay, kBaseRetryDelay * expected_backoff_factor),
            scheduler()->GetTimeUntilNextRequest());
  EXPECT_EQ(num_failures + 1, scheduler()->GetNumConsecutiveFailures());
}

TEST_F(NearbySchedulerBaseTest, ConnectivityChange_RequiresConnectivity) {
  SetNetworkConnection(/*online=*/false);
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();

  RunPendingRequest();

  // Although the timer triggered, the owner is not notified because the request
  // requires network connectivity.
  EXPECT_EQ(0u, on_request_call_count());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());

  // Connectivity is established and pending task is rescheduled.
  SetNetworkConnection(/*online=*/true);
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, ConnectivityChange_DoesNotRequireConnectivity) {
  SetNetworkConnection(/*online=*/false);
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/false);
  scheduler()->MakeImmediateRequest();
  StartScheduling();

  // The scheduler is configured to ignore network connectivity.
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, StopScheduling_BeforeTimerFires) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();

  StartScheduling();
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());

  StopScheduling();
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());

  // Timer is still fired but owner is not notified.
  FastForward(kZeroTimeDelta);
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());

  // Scheduling restarts and pending task is rescheduled.
  StartScheduling();
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, StopScheduling_BeforeResultIsHandled) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();

  StartScheduling();
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());

  StopScheduling();
  EXPECT_TRUE(scheduler()->IsWaitingForResult());

  // Although scheduling is stopped, the result can still be handled. No further
  // requests will be scheduled though.
  FinishPendingRequest(/*success=*/true);
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbySchedulerBaseTest, RestoreRequest_InProgress) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  EXPECT_TRUE(scheduler()->IsWaitingForResult());
  DestroyScheduler();

  // On startup, set a pending immediate request because there was an
  // in-progress request at the time of shutdown.
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  RunPendingRequest();
  EXPECT_EQ(2u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, RestoreRequest_Pending_Immediate) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());
  DestroyScheduler();

  // On startup, set a pending immediate request because there was a pending
  // immediate request at the time of shutdown.
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(kZeroTimeDelta, scheduler()->GetTimeUntilNextRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, RestoreRequest_Pending_FailureRetry) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();

  // Fail three times then destroy scheduler.
  for (size_t num_failures = 0; num_failures < 3; ++num_failures) {
    RunPendingRequest();
    EXPECT_EQ(num_failures + 1, on_request_call_count());
    FinishPendingRequest(/*success=*/false);
  }
  base::TimeDelta intial_time_until_next_request =
      *scheduler()->GetTimeUntilNextRequest();
  EXPECT_EQ(4 * kBaseRetryDelay, intial_time_until_next_request);
  DestroyScheduler();

  // 1s elapses while there is no scheduler. When the scheduler is recreated,
  // the retry request is rescheduled, accounting for the elapsed time.
  base::TimeDelta elapsed_time = base::Seconds(1);
  FastForward(elapsed_time);
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  EXPECT_EQ(intial_time_until_next_request - elapsed_time,
            scheduler()->GetTimeUntilNextRequest());
  EXPECT_EQ(3u, scheduler()->GetNumConsecutiveFailures());
  RunPendingRequest();
  EXPECT_EQ(4u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbySchedulerBaseTest, RestoreSchedulingData) {
  // Succeed immediately, then fail once before destroying scheduler.
  base::Time expected_last_success_time = Now() + base::Seconds(100);
  FastForward(expected_last_success_time - Now());
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();
  RunPendingRequest();
  EXPECT_EQ(1u, on_request_call_count());
  FinishPendingRequest(/*success=*/true);
  scheduler()->MakeImmediateRequest();
  RunPendingRequest();
  EXPECT_EQ(2u, on_request_call_count());
  FinishPendingRequest(/*success=*/false);
  DestroyScheduler();

  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(expected_last_success_time, scheduler()->GetLastSuccessTime());
  EXPECT_EQ(kBaseRetryDelay, scheduler()->GetTimeUntilNextRequest());
  EXPECT_EQ(1u, scheduler()->GetNumConsecutiveFailures());
}

}  // namespace ash::nearby
