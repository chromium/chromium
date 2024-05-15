// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_availability_operation.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_connection_preserver.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace ash::tether {

namespace {

DeviceStatus CreateFakeDeviceStatus() {
  return CreateTestDeviceStatus("Google Fi", 75 /* battery_percentage */,
                                4 /* connection_strength */);
}

}  // namespace

class TetherAvailabilityOperationTest : public testing::Test {
 public:
  TetherAvailabilityOperationTest(const TetherAvailabilityOperationTest&) =
      delete;
  TetherAvailabilityOperationTest& operator=(
      const TetherAvailabilityOperationTest&) = delete;

 protected:
  TetherAvailabilityOperationTest()
      : tether_host_(TetherHost(multidevice::CreateRemoteDeviceRefForTest())) {}

  void SetUp() override {
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();

    operation_ = std::make_unique<TetherAvailabilityOperation>(
        tether_host_,
        base::BindOnce(&TetherAvailabilityOperationTest::OnResponse,
                       weak_ptr_factory_.GetWeakPtr()),
        fake_host_connection_factory_.get(),
        &mock_tether_host_response_recorder_, &fake_connection_preserver_);

    test_clock_.SetNow(base::Time::UnixEpoch());
    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    operation_->SetTestDoubles(&test_clock_, test_task_runner_);
  }

  void OnResponse(std::optional<ScannedDeviceInfo> result) {
    received_result_ = result;
  }

  std::optional<ScannedDeviceInfo> received_result_;
  const TetherHost tether_host_;

  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;
  StrictMock<MockTetherHostResponseRecorder>
      mock_tether_host_response_recorder_;
  FakeConnectionPreserver fake_connection_preserver_;

  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock test_clock_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<TetherAvailabilityOperation> operation_;
  base::WeakPtrFactory<TetherAvailabilityOperationTest> weak_ptr_factory_{this};
};

TEST_F(TetherAvailabilityOperationTest,
       SendsTetherAvailabilityRequestOnceAuthenticated) {
  // Setup the host connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Start the operation.
  operation_->Initialize();

  // Verify the TetherAvailabilityRequest message is sent.
  auto message_wrapper =
      std::make_unique<MessageWrapper>(TetherAvailabilityRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();

  auto& sent_messages = fake_host_connection_factory_
                            ->GetActiveConnection(tether_host_.GetDeviceId())
                            ->sent_messages();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first->ToRawMessage());
}

TEST_F(TetherAvailabilityOperationTest, RecordsResponseDuration) {
  // Setup the host connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Start the operation.
  operation_->Initialize();

  static constexpr base::TimeDelta kTetherAvailabilityResponseTime =
      base::Seconds(3);

  // Advance the clock in order to verify a non-zero response duration is
  // recorded and verified (below).
  test_clock_.Advance(kTetherAvailabilityResponseTime);

  std::unique_ptr<MessageWrapper> message(
      new MessageWrapper(TetherAvailabilityResponse()));
  operation_->OnMessageReceived(std::move(message));

  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.TetherAvailabilityResponseDuration",
      kTetherAvailabilityResponseTime, 1);
}

// Tests that the TetherAvailabilityOperation does not record a potential tether
// connection after receiving an error response.
TEST_F(TetherAvailabilityOperationTest, ErrorResponses) {
  const TetherAvailabilityResponse_ResponseCode kErrorResponseCodes[] = {
      TetherAvailabilityResponse_ResponseCode_UNKNOWN_ERROR,
      TetherAvailabilityResponse_ResponseCode_NO_RECEPTION,
      TetherAvailabilityResponse_ResponseCode_NO_SIM_CARD};
  // Setup the host connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Start the operation.
  operation_->Initialize();

  for (auto response_code : kErrorResponseCodes) {
    // No response should be recorded.
    EXPECT_CALL(mock_tether_host_response_recorder_,
                RecordSuccessfulTetherAvailabilityResponse(_))
        .Times(0);

    // Respond with the error code.
    TetherAvailabilityResponse response;
    response.set_response_code(response_code);
    std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
    operation_->OnMessageReceived(std::move(message));

    // Connection is not preserved.
    bool connection_preserved =
        !fake_connection_preserver_
             .last_requested_preserved_connection_device_id()
             .empty();
    EXPECT_FALSE(connection_preserved);
    EXPECT_FALSE(received_result_.has_value());
  }
}

// Tests that the observer is notified of the list of devices whose
// notifications are disabled each time a new response is received.
TEST_F(TetherAvailabilityOperationTest, NotificationsDisabled) {
  // No response should be recorded.
  EXPECT_CALL(mock_tether_host_response_recorder_,
              RecordSuccessfulTetherAvailabilityResponse(_))
      .Times(0);

  // Respond with the error code.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_LEGACY);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message));

  test_task_runner_->RunUntilIdle();

  // Connection is not preserved.
  bool connection_preserved =
      !fake_connection_preserver_
           .last_requested_preserved_connection_device_id()
           .empty();
  EXPECT_FALSE(connection_preserved);
  EXPECT_FALSE(received_result_.value().notifications_enabled);
}

TEST_F(TetherAvailabilityOperationTest,
       NotificationsDisabledWithNotificationChannel) {
  // No response should be recorded.
  EXPECT_CALL(mock_tether_host_response_recorder_,
              RecordSuccessfulTetherAvailabilityResponse(_))
      .Times(0);

  // Respond with the error code.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_WITH_NOTIFICATION_CHANNEL);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message));

  test_task_runner_->RunUntilIdle();

  // Connection is not preserved.
  bool connection_preserved =
      !fake_connection_preserver_
           .last_requested_preserved_connection_device_id()
           .empty();
  EXPECT_FALSE(connection_preserved);
  EXPECT_FALSE(received_result_.value().notifications_enabled);
}

TEST_F(TetherAvailabilityOperationTest, TetherAvailable) {
  // The scanned device is recorded.
  EXPECT_CALL(
      mock_tether_host_response_recorder_,
      RecordSuccessfulTetherAvailabilityResponse(tether_host_.GetDeviceId()));

  // The observer is notified of the scanned device.
  DeviceStatus device_status = CreateFakeDeviceStatus();
  ScannedDeviceInfo scanned_device(
      tether_host_.GetDeviceId(), tether_host_.GetName(), device_status,
      false /* setup_required */, /*notifications_enabled=*/true);

  // Respond with TETHER_AVAILABLE response code and the device info and status.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE);
  response.mutable_device_status()->CopyFrom(device_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message));

  test_task_runner_->RunUntilIdle();

  // Connection is preserved.
  EXPECT_EQ(tether_host_.GetDeviceId(),
            fake_connection_preserver_
                .last_requested_preserved_connection_device_id());

  EXPECT_TRUE(received_result_.has_value());
  EXPECT_EQ(received_result_.value(), scanned_device);
}

TEST_F(TetherAvailabilityOperationTest, LastProvisioningFailed) {
  // The scanned device is recorded.
  EXPECT_CALL(
      mock_tether_host_response_recorder_,
      RecordSuccessfulTetherAvailabilityResponse(tether_host_.GetDeviceId()));

  // The observer is notified of the scanned device.
  DeviceStatus device_status = CreateFakeDeviceStatus();
  ScannedDeviceInfo scanned_device(
      tether_host_.GetDeviceId(), tether_host_.GetName(), device_status,
      false /* setup_required */, /*notifications_enabled=*/true);
  std::vector<ScannedDeviceInfo> scanned_devices({scanned_device});

  // Respond with TETHER_AVAILABLE response code and the device info and status.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_LAST_PROVISIONING_FAILED);
  response.mutable_device_status()->CopyFrom(device_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message));

  // Connection is preserved.
  EXPECT_EQ(tether_host_.GetDeviceId(),
            fake_connection_preserver_
                .last_requested_preserved_connection_device_id());

  test_task_runner_->RunUntilIdle();

  EXPECT_EQ(scanned_device, received_result_.value());
}

TEST_F(TetherAvailabilityOperationTest, SetupRequired) {
  // The scanned device is recorded.
  EXPECT_CALL(
      mock_tether_host_response_recorder_,
      RecordSuccessfulTetherAvailabilityResponse(tether_host_.GetDeviceId()));

  // The observer is notified that the scanned device has the |setup_required|
  // flag set.
  DeviceStatus device_status = CreateFakeDeviceStatus();
  ScannedDeviceInfo scanned_device(
      tether_host_.GetDeviceId(), tether_host_.GetName(), device_status,
      true /* setup_required */, /*notifications_enabled=*/true);
  std::vector<ScannedDeviceInfo> scanned_devices({scanned_device});

  // Respond with SETUP_NEEDED response code and the device info and status.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED);
  response.mutable_device_status()->CopyFrom(device_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message));

  test_task_runner_->RunUntilIdle();

  // Connection is preserved.
  EXPECT_EQ(tether_host_.GetDeviceId(),
            fake_connection_preserver_
                .last_requested_preserved_connection_device_id());
  EXPECT_EQ(scanned_device, received_result_.value());
}

}  // namespace ash::tether
