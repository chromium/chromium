// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {
namespace {
constexpr auto kLatencyDelta = base::Milliseconds(500u);
}  // namespace

class PhoneHubUiReadinessRecorderTest : public testing::Test {
 protected:
  PhoneHubUiReadinessRecorderTest() = default;
  PhoneHubUiReadinessRecorderTest(const PhoneHubUiReadinessRecorderTest&) =
      delete;
  PhoneHubUiReadinessRecorderTest& operator=(
      const PhoneHubUiReadinessRecorderTest&) = delete;
  ~PhoneHubUiReadinessRecorderTest() override = default;

  void SetUp() override {
    feature_status_provider_ = std::make_unique<FakeFeatureStatusProvider>();
    connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    recorder_ = std::make_unique<PhoneHubUiReadinessRecorder>(
        feature_status_provider_.get(), connection_manager_.get());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<FakeFeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<secure_channel::FakeConnectionManager> connection_manager_;
  std::unique_ptr<PhoneHubUiReadinessRecorder> recorder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PhoneHubUiReadinessRecorderTest, RecordMessageLatency) {
  feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  recorder_->RecordCrosStateMessageSent();
  task_environment_.FastForwardBy(kLatencyDelta);
  recorder_->RecordPhoneStatusSnapShotReceived();
  histogram_tester_.ExpectTimeBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Latency", kLatencyDelta,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 1);
  EXPECT_TRUE(recorder_->message_sent_timestamp_.is_null());

  feature_status_provider_->SetStatus(FeatureStatus::kEnabledButDisconnected);
  recorder_->RecordCrosStateMessageSent();
  EXPECT_FALSE(recorder_->is_cros_state_message_sent_);

  recorder_->RecordPhoneStatusSnapShotReceived();
  EXPECT_FALSE(recorder_->is_phone_status_snapshot_processed_);
}

TEST_F(PhoneHubUiReadinessRecorderTest, RecordMessageSuccess) {
  feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  recorder_->RecordCrosStateMessageSent();

  feature_status_provider_->SetStatus(FeatureStatus::kUnavailableBluetoothOff);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 0);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", false, 1);
  EXPECT_FALSE(recorder_->is_cros_state_message_sent_);
  EXPECT_FALSE(recorder_->is_phone_status_snapshot_processed_);

  feature_status_provider_->SetStatus(FeatureStatus::kEnabledButDisconnected);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 0);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", false, 1);

  feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  recorder_->RecordCrosStateMessageSent();
  recorder_->RecordPhoneStatusSnapShotReceived();
  feature_status_provider_->SetStatus(FeatureStatus::kEnabledButDisconnected);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", false, 1);
}

TEST_F(PhoneHubUiReadinessRecorderTest, RecordUiReadyLatency) {
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(recorder_->connection_flow_state_,
            ConnectionFlowState::kSecureChannelConnected);

  recorder_
      ->RecordCrosStateMessageSent();  // simulate sending cros state message
  EXPECT_EQ(recorder_->connection_flow_state_,
            ConnectionFlowState::kCrosStateMessageSent);

  recorder_->RecordPhoneStatusSnapShotReceived();  // simulate receiving cros
                                                   // state message
  EXPECT_EQ(recorder_->connection_flow_state_,
            ConnectionFlowState::kPhoneSnapShotReceivedButNoPhoneModelSet);

  task_environment_.FastForwardBy(kLatencyDelta);

  recorder_->RecordPhoneHubUiConnected();  // simulate Phone Hub ui state is
                                           // updated to connected.
  EXPECT_EQ(recorder_->connection_flow_state_,
            ConnectionFlowState::kUiConnected);
  histogram_tester_.ExpectTimeBucketCount("PhoneHub.UiReady.Latency",
                                          kLatencyDelta,
                                          /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount("PhoneHub.UiReady.Result",
                                      ConnectionFlowState::kUiConnected, 1);
}

TEST_F(PhoneHubUiReadinessRecorderTest,
       RecordUiReainessFailedAfterSecureChannelConnected) {
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(recorder_->connection_flow_state_,
            ConnectionFlowState::kSecureChannelConnected);
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnecting);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.UiReady.Result", ConnectionFlowState::kSecureChannelConnected,
      1);
}

TEST_F(PhoneHubUiReadinessRecorderTest,
       RecordUiReainessFailedAfterCrosStateMessageSent) {
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  recorder_
      ->RecordCrosStateMessageSent();  // simulate sending cros state message
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kDisconnected);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.UiReady.Result", ConnectionFlowState::kCrosStateMessageSent, 1);
}

TEST_F(PhoneHubUiReadinessRecorderTest,
       RecordUiReainessFailedAfterPhoneSnapShotReceived) {
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  recorder_
      ->RecordCrosStateMessageSent();  // simulate sending cros state message
  recorder_->RecordPhoneStatusSnapShotReceived();  // simulate receiving cros
                                                   // state message
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kDisconnected);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.UiReady.Result",
      ConnectionFlowState::kPhoneSnapShotReceivedButNoPhoneModelSet, 1);
}

TEST_F(PhoneHubUiReadinessRecorderTest,
       RecordUiReainessDisconnectAfterSuccessfulConnection) {
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  recorder_
      ->RecordCrosStateMessageSent();  // simulate sending cros state message
  recorder_->RecordPhoneStatusSnapShotReceived();  // simulate receiving cros
                                                   // state message
  recorder_->RecordPhoneHubUiConnected();  // simulate Phone Hub ui state is
                                           // updated to connected.
  EXPECT_EQ(recorder_->connection_flow_state_,
            ConnectionFlowState::kUiConnected);
  histogram_tester_.ExpectBucketCount("PhoneHub.UiReady.Result",
                                      ConnectionFlowState::kUiConnected, 1);
  connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kDisconnected);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.UiReady.Result", ConnectionFlowState::kUiConnected,
      1);  // If disconnection happens after UI is fully ready, we should not
           // record any data to histogram.
}

}  // namespace ash::phonehub
