// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetch_throttler.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_fetch_throttler_delegate.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {
namespace {

class MockAffiliationFetchThrottlerDelegate
    : public AffiliationFetchThrottlerDelegate {
 public:
  // The |tick_clock| should outlive this instance.
  explicit MockAffiliationFetchThrottlerDelegate(
      const base::TickClock* tick_clock)
      : tick_clock_(tick_clock),
        emulated_return_value_(true),
        can_send_count_(0u) {}

  MockAffiliationFetchThrottlerDelegate(
      const MockAffiliationFetchThrottlerDelegate&) = delete;
  MockAffiliationFetchThrottlerDelegate& operator=(
      const MockAffiliationFetchThrottlerDelegate&) = delete;

  ~MockAffiliationFetchThrottlerDelegate() override {
    EXPECT_EQ(0u, can_send_count_);
  }

  void set_emulated_return_value(bool value) { emulated_return_value_ = value; }
  void reset_can_send_count() { can_send_count_ = 0u; }
  size_t can_send_count() const { return can_send_count_; }
  base::TimeTicks last_can_send_time() const { return last_can_send_time_; }

  // AffiliationFetchThrottlerDelegate:
  bool OnCanSendNetworkRequest() override {
    ++can_send_count_;
    last_can_send_time_ = tick_clock_->NowTicks();
    return emulated_return_value_;
  }

 private:
  raw_ptr<const base::TickClock> tick_clock_;
  bool emulated_return_value_;
  size_t can_send_count_;
  base::TimeTicks last_can_send_time_;
};

}  // namespace

class AffiliationFetchThrottlerTest : public testing::Test {
 public:
  AffiliationFetchThrottlerTest() { SimulateHasNetworkConnectivity(true); }

  AffiliationFetchThrottlerTest(const AffiliationFetchThrottlerTest&) = delete;
  AffiliationFetchThrottlerTest& operator=(
      const AffiliationFetchThrottlerTest&) = delete;

  std::unique_ptr<AffiliationFetchThrottler> CreateThrottler() {
    return std::make_unique<AffiliationFetchThrottler>(
        &mock_delegate_, task_runner_,
        network::TestNetworkConnectionTracker::GetInstance(),
        task_runner_->GetMockTickClock());
  }

  void SimulateHasNetworkConnectivity(bool has_connectivity) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        has_connectivity ? network::mojom::ConnectionType::CONNECTION_ETHERNET
                         : network::mojom::ConnectionType::CONNECTION_NONE);
    task_environment_.RunUntilIdle();
  }

  // Runs the task runner until no tasks remain, and asserts that by this time,
  // OnCanSendNetworkRequest() will have been called exactly once, with a delay
  // between |min_delay_ms| and |max_delay_ms|, modulo 0.5 ms to allow for
  // floating point errors. When OnCanSendNetworkRequest() is called, the mock
  // will return |emulated_return_value|. This value normally indicates whether
  // or not a request was actually issued in response to the call.
  void AssertReleaseInBetween(bool emulated_return_value,
                              double min_delay_ms,
                              double max_delay_ms) {
    ASSERT_EQ(0u, mock_delegate_.can_send_count());
    base::TimeTicks ticks_at_start = task_runner_->NowTicks();
    mock_delegate_.set_emulated_return_value(emulated_return_value);
    task_runner_->FastForwardUntilNoTasksRemain();
    ASSERT_EQ(1u, mock_delegate_.can_send_count());
    base::TimeDelta delay =
        mock_delegate_.last_can_send_time() - ticks_at_start;
    EXPECT_LE(min_delay_ms - 1, delay.InMillisecondsF());
    EXPECT_GE(max_delay_ms + 1, delay.InMillisecondsF());
    mock_delegate_.reset_can_send_count();
  }

  // Runs the task runner for |secs| and asserts that OnCanSendNetworkRequest()
  // will not have been called by the end of this period.
  void AssertNoReleaseForSecs(int64_t secs) {
    task_runner_->FastForwardBy(base::Seconds(secs));
    ASSERT_EQ(0u, mock_delegate_.can_send_count());
  }

  // Runs the task runner until no tasks remain, and asserts that
  // OnCanSendNetworkRequest() will not have been called.
  void AssertNoReleaseUntilNoTasksRemain() {
    task_runner_->FastForwardUntilNoTasksRemain();
    ASSERT_EQ(0u, mock_delegate_.can_send_count());
  }

  size_t GetPendingTaskCount() const {
    return task_runner_->GetPendingTaskCount();
  }

 private:
  // Needed because NetworkConnectionTracker uses base::ObserverList, which
  // notifies observers on the sequence from which they have registered.
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  MockAffiliationFetchThrottlerDelegate mock_delegate_{
      task_runner_->GetMockTickClock()};
};

TEST_F(AffiliationFetchThrottlerTest, SuccessfulRequests) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));

  // Signal while request is in flight should be ignored.
  throttler->SignalNetworkRequestNeeded();
  AssertNoReleaseUntilNoTasksRemain();
  throttler->InformOfNetworkRequestComplete(true);
  AssertNoReleaseUntilNoTasksRemain();

  // Duplicate the second signal 3 times: still only 1 callback should arrive.
  throttler->SignalNetworkRequestNeeded();
  throttler->SignalNetworkRequestNeeded();
  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
}

TEST_F(AffiliationFetchThrottlerTest, FailedRequests) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
  throttler->InformOfNetworkRequestComplete(false);

  // The request after the first failure should be delayed by |initial_delay_ms|
  // spread out over Uniform(1 - |jitter_factor|, 1).
  throttler->SignalNetworkRequestNeeded();
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kPolicy.initial_delay_ms * (1 - kPolicy.jitter_factor),
      kPolicy.initial_delay_ms));
  throttler->InformOfNetworkRequestComplete(true);

  // After a successful request, the next one should be released immediately.
  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
  throttler->InformOfNetworkRequestComplete(false);

  // In general, the request after the n-th failure should be delayed by
  //   |multiply_factor| ^ (n-1) * |initial_delay_ms|,
  // spread out over Uniform(1 - |jitter_factor|, 1), up until
  //   |maximum_backoff_ms|
  // is reached.
  for (int num_failures = 1; num_failures < 100; ++num_failures) {
    throttler->SignalNetworkRequestNeeded();
    double max_delay_ms = kPolicy.initial_delay_ms *
                          pow(kPolicy.multiply_factor, num_failures - 1);
    double min_delay_ms = max_delay_ms * (1 - kPolicy.jitter_factor);
    if (max_delay_ms > kPolicy.maximum_backoff_ms)
      max_delay_ms = kPolicy.maximum_backoff_ms;
    if (min_delay_ms > kPolicy.maximum_backoff_ms)
      min_delay_ms = kPolicy.maximum_backoff_ms;
    ASSERT_NO_FATAL_FAILURE(
        AssertReleaseInBetween(true, min_delay_ms, max_delay_ms));
    throttler->InformOfNetworkRequestComplete(false);
  }
}

TEST_F(AffiliationFetchThrottlerTest, OnCanSendNetworkRequestReturnsFalse) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  // A need for a network request is signaled, but as OnCanSendNetworkRequest()
  // is called, the implementation returns false to indicate that the request
  // will not be needed after all. InformOfNetworkRequestComplete() must not be
  // called in this case.
  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(false, 0, 0));

  // A subsequent signaling, however, should result in OnCanSendNetworkRequest()
  // being called immediately.
  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
}

TEST_F(AffiliationFetchThrottlerTest, GracePeriodAfterConnectivityIsRestored) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());
  SimulateHasNetworkConnectivity(false);

  // After connectivity is restored, the first request should be delayed by the
  // grace period, spread out over Uniform(1 - |jitter_factor|, 1).
  throttler->SignalNetworkRequestNeeded();
  AssertNoReleaseUntilNoTasksRemain();

  SimulateHasNetworkConnectivity(true);
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kGraceMs * (1 - kPolicy.jitter_factor), kGraceMs));
  throttler->InformOfNetworkRequestComplete(true);

  // The next request should not be delayed.
  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
}

// Same as GracePeriodAfterConnectivityIsRestored, but the network comes back
// just before SignalNetworkRequestNeeded() is called.
TEST_F(AffiliationFetchThrottlerTest, GracePeriodAfterConnectivityIsRestored2) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());
  SimulateHasNetworkConnectivity(false);

  SimulateHasNetworkConnectivity(true);
  throttler->SignalNetworkRequestNeeded();
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kGraceMs * (1 - kPolicy.jitter_factor), kGraceMs));
  throttler->InformOfNetworkRequestComplete(true);

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
}

TEST_F(AffiliationFetchThrottlerTest, ConnectivityLostDuringBackoff) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
  throttler->InformOfNetworkRequestComplete(false);

  throttler->SignalNetworkRequestNeeded();
  SimulateHasNetworkConnectivity(false);

  // Let the exponential backoff delay expire, and verify nothing happens.
  AssertNoReleaseUntilNoTasksRemain();

  // Verify that the request is, however, sent after the normal grace period
  // once connectivity is restored.
  SimulateHasNetworkConnectivity(true);
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kGraceMs * (1 - kPolicy.jitter_factor), kGraceMs));
  throttler->InformOfNetworkRequestComplete(true);
}

TEST_F(AffiliationFetchThrottlerTest,
       ConnectivityLostAndRestoredDuringBackoff) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
  throttler->InformOfNetworkRequestComplete(false);

  throttler->SignalNetworkRequestNeeded();
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kPolicy.initial_delay_ms * (1 - kPolicy.jitter_factor),
      kPolicy.initial_delay_ms));
  throttler->InformOfNetworkRequestComplete(false);

  SimulateHasNetworkConnectivity(false);
  SimulateHasNetworkConnectivity(true);

  // This test expects that the exponential backoff interval after the 2nd error
  // is larger than the normal grace period after connectivity is restored.
  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  EXPECT_PRED_FORMAT2(testing::DoubleLE, kGraceMs,
                      kPolicy.initial_delay_ms * kPolicy.multiply_factor);

  // The release should come after the longest of the two intervals expire.
  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kPolicy.initial_delay_ms * kPolicy.multiply_factor *
                (1 - kPolicy.jitter_factor),
      kPolicy.initial_delay_ms * kPolicy.multiply_factor));
  throttler->InformOfNetworkRequestComplete(false);
}

TEST_F(AffiliationFetchThrottlerTest, FlakyConnectivity) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
  throttler->InformOfNetworkRequestComplete(false);

  // Run for a total of 5 grace periods and simulate connectivity being lost and
  // restored every second. This verifies that a flaky connection will not flood
  // the task queue with lots of of tasks and also that release will not happen
  // while the connection is flaky even once the first grace period has expired.
  throttler->SignalNetworkRequestNeeded();
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  int64_t five_grace_periods_secs =
      kGraceMs * 5 / base::Time::kMillisecondsPerSecond;
  for (int64_t t = 0; t < five_grace_periods_secs; ++t) {
    SimulateHasNetworkConnectivity(false);
    AssertNoReleaseForSecs(1);
    SimulateHasNetworkConnectivity(true);
    EXPECT_EQ(1u, GetPendingTaskCount());
  }
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kGraceMs * (1 - kPolicy.jitter_factor), kGraceMs));
}

TEST_F(AffiliationFetchThrottlerTest, ConnectivityLostDuringRequest) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));

  SimulateHasNetworkConnectivity(false);
  AssertNoReleaseUntilNoTasksRemain();
  throttler->InformOfNetworkRequestComplete(false);
  AssertNoReleaseUntilNoTasksRemain();
  throttler->SignalNetworkRequestNeeded();
  AssertNoReleaseUntilNoTasksRemain();

  SimulateHasNetworkConnectivity(true);

  // Verify that the next request is released after the normal grace period.
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kGraceMs * (1 - kPolicy.jitter_factor), kGraceMs));
  throttler->InformOfNetworkRequestComplete(true);
}

TEST_F(AffiliationFetchThrottlerTest,
       ConnectivityLostAndRestoredDuringRequest) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));

  SimulateHasNetworkConnectivity(false);
  AssertNoReleaseUntilNoTasksRemain();
  SimulateHasNetworkConnectivity(true);
  AssertNoReleaseUntilNoTasksRemain();
  throttler->InformOfNetworkRequestComplete(true);

  // Even though the previous request succeeded, the next request should still
  // be held back for the normal grace period after connection is restored.
  throttler->SignalNetworkRequestNeeded();
  const auto& kPolicy = AffiliationFetchThrottler::kBackoffPolicy;
  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(
      true, kGraceMs * (1 - kPolicy.jitter_factor), kGraceMs));
  throttler->InformOfNetworkRequestComplete(true);
}

TEST_F(AffiliationFetchThrottlerTest,
       ConnectivityLostAndRestoredDuringRequest2) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));

  SimulateHasNetworkConnectivity(false);
  AssertNoReleaseUntilNoTasksRemain();
  SimulateHasNetworkConnectivity(true);

  const int64_t& kGraceMs =
      AffiliationFetchThrottler::kGracePeriodAfterReconnectMs;
  AssertNoReleaseForSecs(kGraceMs / base::Time::kMillisecondsPerSecond);
  throttler->InformOfNetworkRequestComplete(true);

  // The next request should not be held back.
  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
}

TEST_F(AffiliationFetchThrottlerTest, InstanceDestroyedWhileInBackoff) {
  std::unique_ptr<AffiliationFetchThrottler> throttler(CreateThrottler());

  throttler->SignalNetworkRequestNeeded();
  ASSERT_NO_FATAL_FAILURE(AssertReleaseInBetween(true, 0, 0));
  throttler->InformOfNetworkRequestComplete(false);

  throttler->SignalNetworkRequestNeeded();
  throttler.reset();
  // We expect the task to be cancelled.
  EXPECT_EQ(0u, GetPendingTaskCount());
  AssertNoReleaseUntilNoTasksRemain();
}

}  // namespace affiliations
