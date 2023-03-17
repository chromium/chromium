// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/ping_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/phonehub/fake_message_receiver.h"
#include "chromeos/ash/components/phonehub/fake_message_sender.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {

namespace {

constexpr auto kLatencyDelta = base::Milliseconds(123u);
constexpr char kPingManagerLatencyHistogramName[] =
    "PhoneHub.PhoneAvailabilityCheck.Latency";
constexpr char kPingManagerPingResultHistogramName[] =
    "PhoneHub.PhoneAvailabilityCheck.Result";

}  // namespace

class PingManagerImplTest : public testing::Test {
 public:
  PingManagerImplTest() = default;
  PingManagerImplTest(const PingManagerImplTest&) = delete;
  PingManagerImplTest& operator=(const PingManagerImplTest&) = delete;
  ~PingManagerImplTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPhoneHubPingOnBubbleOpen},
        /*disabled_features=*/{});

    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    ping_manager_ = std::make_unique<PingManagerImpl>(
        fake_connection_manager_.get(), &fake_message_receiver_,
        &fake_message_sender_);

    ping_manager_->set_is_ping_supported_by_phone(true);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<PingManagerImpl> ping_manager_;
  FakeMessageReceiver fake_message_receiver_;
  FakeMessageSender fake_message_sender_;
};

TEST_F(PingManagerImplTest, OnPhoneStatusSnapshotReceivedPingSupported) {
  ping_manager_->set_is_ping_supported_by_phone(false);
  EXPECT_FALSE(ping_manager_->is_ping_supported_by_phone());

  auto config = std::make_unique<proto::FeatureSetupConfig>();
  config->set_ping_capability_supported(true);

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_allocated_feature_setup_config(
      config.release());

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());

  // Simulate receiving a message
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  EXPECT_TRUE(ping_manager_->is_ping_supported_by_phone());
}

TEST_F(PingManagerImplTest, OnPhoneStatusSnapshotReceivedPingNotSupported) {
  auto config = std::make_unique<proto::FeatureSetupConfig>();
  config->set_ping_capability_supported(false);

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_allocated_feature_setup_config(
      config.release());

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());

  // Simulate receiving a message
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  EXPECT_FALSE(ping_manager_->is_ping_supported_by_phone());
}

TEST_F(PingManagerImplTest, OnPhoneStatusUpdateReceivedPingSupported) {
  ping_manager_->set_is_ping_supported_by_phone(false);
  EXPECT_FALSE(ping_manager_->is_ping_supported_by_phone());

  auto config = std::make_unique<proto::FeatureSetupConfig>();
  config->set_ping_capability_supported(true);

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_allocated_feature_setup_config(
      config.release());

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  // Simulate receiving a message
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_TRUE(ping_manager_->is_ping_supported_by_phone());
}

TEST_F(PingManagerImplTest, OnPhoneStatusUpdateReceivedPingNotSupported) {
  auto config = std::make_unique<proto::FeatureSetupConfig>();
  config->set_ping_capability_supported(false);

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_allocated_feature_setup_config(
      config.release());

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  // Simulate receiving a message
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_FALSE(ping_manager_->is_ping_supported_by_phone());
}

TEST_F(PingManagerImplTest, OnPhoneStatusUpdateReceivedNoFeatureSetupConfig) {
  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  // Simulate receiving a message
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_FALSE(ping_manager_->is_ping_supported_by_phone());
}

TEST_F(PingManagerImplTest, OnPingResponseReceived) {
  base::HistogramTester histogram_tester;

  ping_manager_->set_is_waiting_for_response(true);
  EXPECT_TRUE(ping_manager_->is_waiting_for_response());

  // Simulate receiving a message
  fake_message_receiver_.NotifyPingResponseReceived();

  histogram_tester.ExpectBucketCount(kPingManagerPingResultHistogramName, true,
                                     1);

  EXPECT_FALSE(ping_manager_->is_waiting_for_response());
  EXPECT_FALSE(ping_manager_->IsPingTimeoutTimerRunning());
}

TEST_F(PingManagerImplTest, SendPingRequest) {
  proto::PingRequest request;

  ping_manager_->SendPingRequest();

  EXPECT_EQ(1u, fake_message_sender_.GetPingRequestCallCount());
  EXPECT_TRUE(ping_manager_->is_waiting_for_response());
  EXPECT_TRUE(ping_manager_->IsPingTimeoutTimerRunning());
}

TEST_F(PingManagerImplTest, SendPingRequestNotSupported) {
  ping_manager_->set_is_ping_supported_by_phone(false);

  ping_manager_->SendPingRequest();

  EXPECT_EQ(0u, fake_message_sender_.GetPingRequestCallCount());
  EXPECT_FALSE(ping_manager_->is_waiting_for_response());
  EXPECT_FALSE(ping_manager_->IsPingTimeoutTimerRunning());
}

TEST_F(PingManagerImplTest, SendPingRequestWaitingForResponse) {
  ping_manager_->set_is_waiting_for_response(true);

  ping_manager_->SendPingRequest();

  EXPECT_EQ(0u, fake_message_sender_.GetPingRequestCallCount());
  EXPECT_TRUE(ping_manager_->is_waiting_for_response());
  EXPECT_FALSE(ping_manager_->IsPingTimeoutTimerRunning());
}

TEST_F(PingManagerImplTest, OnPingTimerFired) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(0u, fake_connection_manager_->num_disconnect_calls());
  EXPECT_FALSE(ping_manager_->is_waiting_for_response());
  EXPECT_FALSE(ping_manager_->IsPingTimeoutTimerRunning());

  ping_manager_->SendPingRequest();

  EXPECT_EQ(0u, fake_connection_manager_->num_disconnect_calls());
  EXPECT_EQ(1u, fake_message_sender_.GetPingRequestCallCount());
  EXPECT_TRUE(ping_manager_->is_waiting_for_response());
  EXPECT_TRUE(ping_manager_->IsPingTimeoutTimerRunning());

  task_environment_.FastForwardBy(base::Minutes(2));

  histogram_tester.ExpectBucketCount(kPingManagerPingResultHistogramName, false,
                                     1);

  EXPECT_EQ(1u, fake_connection_manager_->num_disconnect_calls());
  EXPECT_FALSE(ping_manager_->is_waiting_for_response());
  EXPECT_FALSE(ping_manager_->IsPingTimeoutTimerRunning());
}

TEST_F(PingManagerImplTest, SendAndReceivePing) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(ping_manager_->is_waiting_for_response());
  EXPECT_FALSE(ping_manager_->IsPingTimeoutTimerRunning());

  ping_manager_->SendPingRequest();

  EXPECT_EQ(0u, fake_connection_manager_->num_disconnect_calls());
  EXPECT_EQ(1u, fake_message_sender_.GetPingRequestCallCount());
  EXPECT_TRUE(ping_manager_->is_waiting_for_response());
  EXPECT_TRUE(ping_manager_->IsPingTimeoutTimerRunning());

  task_environment_.FastForwardBy(kLatencyDelta);

  // Simulate receiving a message.
  fake_message_receiver_.NotifyPingResponseReceived();

  histogram_tester.ExpectTimeBucketCount(kPingManagerLatencyHistogramName,
                                         kLatencyDelta, 1);
  histogram_tester.ExpectBucketCount(kPingManagerPingResultHistogramName, true,
                                     1);

  EXPECT_FALSE(ping_manager_->is_waiting_for_response());
  EXPECT_FALSE(ping_manager_->IsPingTimeoutTimerRunning());
  EXPECT_EQ(0u, fake_connection_manager_->num_disconnect_calls());
}

}  // namespace ash::phonehub
