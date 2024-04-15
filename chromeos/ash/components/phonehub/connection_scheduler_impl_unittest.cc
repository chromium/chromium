// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/connection_scheduler_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

class ConnectionSchedulerImplTest : public testing::Test {
 protected:
  ConnectionSchedulerImplTest() = default;
  ConnectionSchedulerImplTest(const ConnectionSchedulerImplTest&) = delete;
  ConnectionSchedulerImplTest& operator=(const ConnectionSchedulerImplTest&) =
      delete;
  ~ConnectionSchedulerImplTest() override = default;

  void SetUp() override {
    PhoneHubStructuredMetricsLogger::RegisterPrefs(pref_service_.registry());
    phone_hub_structured_metrics_logger_ =
        std::make_unique<PhoneHubStructuredMetricsLogger>(&pref_service_);
    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>();
  }

  void CreateConnectionScheduler() {
    connection_scheduler_ = std::make_unique<ConnectionSchedulerImpl>(
        fake_connection_manager_.get(), fake_feature_status_provider_.get(),
        phone_hub_structured_metrics_logger_.get());
  }

  base::TimeDelta GetCurrentBackoffDelay() {
    return connection_scheduler_->GetCurrentBackoffDelayTimeForTesting();
  }

  int GetBackoffFailureCount() {
    return connection_scheduler_->GetBackoffFailureCountForTesting();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<PhoneHubStructuredMetricsLogger>
      phone_hub_structured_metrics_logger_;
  std::unique_ptr<ConnectionSchedulerImpl> connection_scheduler_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ConnectionSchedulerImplTest, SuccesssfullyAttemptConnection) {
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow(
      DiscoveryEntryPoint::kUserSignIn);
  // Verify that the ConnectionManager has attempted to connect.
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());

  // Simulate state changes with AttemptConnection().
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledAndConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  // Verify only 1 call to AttemptConnection() was ever made.
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());
  // Verify that we did not attempt a backoff retry.
  EXPECT_EQ(0, GetBackoffFailureCount());
}

TEST_F(ConnectionSchedulerImplTest, FeatureDisabledDoesNotEstablishConnection) {
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisabled);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow(
      DiscoveryEntryPoint::kUserSignIn);
  // Verify that the ConnectionManager did not attempt connection.
  EXPECT_EQ(0u, fake_connection_manager_->num_attempt_connection_calls());
  // Verify that we did not attempt a backoff retry.
  EXPECT_EQ(0, GetBackoffFailureCount());
}

TEST_F(ConnectionSchedulerImplTest, BackoffRetryWithUpdatedConnection) {
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow(
      DiscoveryEntryPoint::kUserSignIn);
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());
  // Simulate state changes with AttemptConnection().
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledAndConnecting);
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(1, GetBackoffFailureCount());

  // Move forward time to the next backoff retry with disconnected status.
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  EXPECT_EQ(2u, fake_connection_manager_->num_attempt_connection_calls());
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledAndConnecting);
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(2, GetBackoffFailureCount());

  // Move forward time to the next backoff retry, this time with connected
  // status.
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  EXPECT_EQ(3u, fake_connection_manager_->num_attempt_connection_calls());
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledAndConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  // Expected no more backoff failures since connection is now established.
  EXPECT_EQ(0, GetBackoffFailureCount());

  // Fast forward time and confirm no other retries have been made.
  task_environment_.FastForwardBy(base::Seconds(100));
  EXPECT_EQ(3u, fake_connection_manager_->num_attempt_connection_calls());
  EXPECT_EQ(0, GetBackoffFailureCount());
  EXPECT_EQ(FeatureStatus::kEnabledAndConnected,
            fake_feature_status_provider_->GetStatus());
}

TEST_F(ConnectionSchedulerImplTest, BackoffRetryWithUpdatedFeatures) {
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow(
      DiscoveryEntryPoint::kUserSignIn);
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());
  // Simulate state changes with AttemptConnection().
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledAndConnecting);
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(1, GetBackoffFailureCount());

  // Simulate the feature status switched to disabled.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisabled);
  // Expect the backoff to reset and never attempt to kickoff another
  // connection.
  EXPECT_EQ(0, GetBackoffFailureCount());
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());
  // Expect that connection has been disconnected.
  EXPECT_EQ(1u, fake_connection_manager_->num_disconnect_calls());

  // Fast forward time and confirm no other retries have been made.
  task_environment_.FastForwardBy(base::Seconds(100));
  EXPECT_EQ(0, GetBackoffFailureCount());
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());

  // Simulate the feature re-enabled and the connection kickoff should start.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  // The next ScheduleConnection() was not caused by a previous failure, expect
  // backoff failure count to not increase.
  EXPECT_EQ(0, GetBackoffFailureCount());

  // Move forward in time and confirm backoff attempted another retry.
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  EXPECT_EQ(2u, fake_connection_manager_->num_attempt_connection_calls());
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledAndConnecting);
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  // The next ScheduleConnection() was caused by a previous failure, expect 1
  // failure count.
  EXPECT_EQ(1, GetBackoffFailureCount());
}

TEST_F(ConnectionSchedulerImplTest, ScheduleConnectionSuspended) {
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  CreateConnectionScheduler();

  // Simulate screen locked and expect no scheduled connections.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kLockOrSuspended);
  // Expect no scheduled connections on screen lock.
  EXPECT_EQ(0, GetBackoffFailureCount());
  EXPECT_EQ(0u, fake_connection_manager_->num_attempt_connection_calls());
  EXPECT_EQ(1u, fake_connection_manager_->num_disconnect_calls());

  // Simulate screen unlocked and expect a scheduled connection.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(0, GetBackoffFailureCount());
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());
}

TEST_F(ConnectionSchedulerImplTest, HostsNotEligible) {
  // Simulate no eligible hosts available. Expect no scheduled connections.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kNotEligibleForFeature);
  CreateConnectionScheduler();

  EXPECT_EQ(0, GetBackoffFailureCount());
  EXPECT_EQ(0u, fake_connection_manager_->num_attempt_connection_calls());

  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);
  // Flip to have eligble hosts available. Expect a scheduled connection.
  EXPECT_EQ(0, GetBackoffFailureCount());
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());
}

}  // namespace phonehub
}  // namespace ash
