// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner_operation.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/tether/fake_connection_preserver.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace chromeos {

namespace tether {

namespace {

class FakeHostScanDevicePrioritizer : public HostScanDevicePrioritizer {
 public:
  FakeHostScanDevicePrioritizer() : HostScanDevicePrioritizer() {}
  ~FakeHostScanDevicePrioritizer() override = default;

  // Simply leave |remote_devices| as-is.
  void SortByHostScanOrder(
      multidevice::RemoteDeviceRefList* remote_devices) const override {}
};

// Used to verify the HostScannerOperation notifies the observer when
// appropriate.
class MockOperationObserver : public HostScannerOperation::Observer {
 public:
  MockOperationObserver() = default;
  ~MockOperationObserver() = default;

  MOCK_METHOD3(OnTetherAvailabilityResponse,
               void(const std::vector<HostScannerOperation::ScannedDeviceInfo>&,
                    const multidevice::RemoteDeviceRefList&,
                    bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOperationObserver);
};

DeviceStatus CreateFakeDeviceStatus() {
  return CreateTestDeviceStatus("Google Fi", 75 /* battery_percentage */,
                                4 /* connection_strength */);
}

}  // namespace

class HostScannerOperationTest : public testing::Test {
 protected:
  HostScannerOperationTest()
      : local_device_(multidevice::RemoteDeviceRefBuilder()
                          .SetPublicKey("local device")
                          .Build()),
        remote_device_(multidevice::CreateRemoteDeviceRefListForTest(1)[0]) {}

  void SetUp() override {
    fake_device_sync_client_.set_local_device_metadata(local_device_);

    operation_ = ConstructOperation();
    operation_->Initialize();

    ConnectAuthenticatedChannelForDevice(remote_device_);
  }

  multidevice::RemoteDeviceRefList GetOperationRemoteDevices(
      HostScannerOperation* operation) const {
    return operation->remote_devices();
  }

  std::unique_ptr<HostScannerOperation> ConstructOperation() {
    EXPECT_CALL(mock_observer_,
                OnTetherAvailabilityResponse(
                    std::vector<HostScannerOperation::ScannedDeviceInfo>(),
                    multidevice::RemoteDeviceRefList(), false));

    auto connection_attempt =
        std::make_unique<secure_channel::FakeConnectionAttempt>();
    connection_attempt_ = connection_attempt.get();
    fake_secure_channel_client_.set_next_listen_connection_attempt(
        remote_device_, local_device_, std::move(connection_attempt));

    auto operation = base::WrapUnique(new HostScannerOperation(
        multidevice::RemoteDeviceRefList({remote_device_}),
        &fake_device_sync_client_, &fake_secure_channel_client_,
        &fake_device_prioritizer_, &mock_tether_host_response_recorder_,
        &fake_connection_preserver_));

    operation->AddObserver(&mock_observer_);

    test_clock_.SetNow(base::Time::UnixEpoch());
    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    operation->SetTestDoubles(&test_clock_, test_task_runner_);

    return operation;
  }

  void ConnectAuthenticatedChannelForDevice(
      multidevice::RemoteDeviceRef remote_device) {
    auto fake_client_channel =
        std::make_unique<secure_channel::FakeClientChannel>();
    connection_attempt_->NotifyConnection(std::move(fake_client_channel));
  }

  const multidevice::RemoteDeviceRef local_device_;
  const multidevice::RemoteDeviceRef remote_device_;

  secure_channel::FakeConnectionAttempt* connection_attempt_;
  device_sync::FakeDeviceSyncClient fake_device_sync_client_;
  secure_channel::FakeSecureChannelClient fake_secure_channel_client_;
  FakeHostScanDevicePrioritizer fake_device_prioritizer_;
  StrictMock<MockTetherHostResponseRecorder>
      mock_tether_host_response_recorder_;
  FakeConnectionPreserver fake_connection_preserver_;

  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock test_clock_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
  MockOperationObserver mock_observer_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<HostScannerOperation> operation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostScannerOperationTest);
};

TEST_F(HostScannerOperationTest,
       SendsTetherAvailabilityRequestOnceAuthenticated) {
  std::unique_ptr<HostScannerOperation> operation = ConstructOperation();
  operation->Initialize();

  // Create the client channel to the remote device.
  auto fake_client_channel =
      std::make_unique<secure_channel::FakeClientChannel>();

  // No requests as a result of creating the client channel.
  auto& sent_messages = fake_client_channel->sent_messages();
  EXPECT_EQ(0u, sent_messages.size());

  // Connect and authenticate the client channel.
  connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Verify the TetherAvailabilityRequest message is sent.
  auto message_wrapper =
      std::make_unique<MessageWrapper>(TetherAvailabilityRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first);
}

TEST_F(HostScannerOperationTest, RecordsResponseDuration) {
  static constexpr base::TimeDelta kTetherAvailabilityResponseTime =
      base::TimeDelta::FromSeconds(3);

  // Advance the clock in order to verify a non-zero response duration is
  // recorded and verified (below).
  test_clock_.Advance(kTetherAvailabilityResponseTime);

  std::unique_ptr<MessageWrapper> message(
      new MessageWrapper(TetherAvailabilityResponse()));
  operation_->OnMessageReceived(std::move(message), remote_device_);

  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.TetherAvailabilityResponseDuration",
      kTetherAvailabilityResponseTime, 1);
}

// Tests that the HostScannerOperation does not record a potential tether
// connection after receiving an error response.
TEST_F(HostScannerOperationTest, ErrorResponses) {
  const TetherAvailabilityResponse_ResponseCode kErrorResponseCodes[] = {
      TetherAvailabilityResponse_ResponseCode_UNKNOWN_ERROR,
      TetherAvailabilityResponse_ResponseCode_NO_RECEPTION,
      TetherAvailabilityResponse_ResponseCode_NO_SIM_CARD};

  for (auto response_code : kErrorResponseCodes) {
    // No response should be recorded.
    EXPECT_CALL(mock_tether_host_response_recorder_,
                RecordSuccessfulTetherAvailabilityResponse(_))
        .Times(0);

    // Observers should not be notified.
    EXPECT_CALL(mock_observer_, OnTetherAvailabilityResponse(
                                    testing::_, testing::_, testing::_))
        .Times(0);

    // Respond with the error code.
    TetherAvailabilityResponse response;
    response.set_response_code(response_code);
    std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
    operation_->OnMessageReceived(std::move(message), remote_device_);

    // Connection is not preserved.
    bool connection_preserved =
        !fake_connection_preserver_
             .last_requested_preserved_connection_device_id()
             .empty();
    EXPECT_FALSE(connection_preserved);
  }
}

// Tests that the observer is notified of the list of devices whose
// notifications are disabled each time a new response is received.
TEST_F(HostScannerOperationTest, NotificationsDisabled) {
  std::vector<TetherAvailabilityResponse_ResponseCode>
      kNotificationsDisabledResponseCodes = {
          TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_LEGACY,
          TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_WITH_NOTIFICATION_CHANNEL};

  multidevice::RemoteDeviceRefList devices_notifications_disabled;

  for (auto response_code : kNotificationsDisabledResponseCodes) {
    // No response should be recorded.
    EXPECT_CALL(mock_tether_host_response_recorder_,
                RecordSuccessfulTetherAvailabilityResponse(_))
        .Times(0);

    // Because the operation is ongoing, each device contained in the response
    // is added to the list of devices whose notifications are disabled.
    devices_notifications_disabled.push_back(remote_device_);

    // The observer is notified of the list of devices whose notificaitons are
    // disabled.
    EXPECT_CALL(
        mock_observer_,
        OnTetherAvailabilityResponse(
            std::vector<HostScannerOperation::ScannedDeviceInfo>(),
            devices_notifications_disabled, false /* is_final_scan_result */));

    // Respond with the error code.
    TetherAvailabilityResponse response;
    response.set_response_code(response_code);
    std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
    operation_->OnMessageReceived(std::move(message), remote_device_);

    // Connection is not preserved.
    bool connection_preserved =
        !fake_connection_preserver_
             .last_requested_preserved_connection_device_id()
             .empty();
    EXPECT_FALSE(connection_preserved);
  }
}

TEST_F(HostScannerOperationTest, TetherAvailable) {
  // The scanned device is recorded.
  EXPECT_CALL(mock_tether_host_response_recorder_,
              RecordSuccessfulTetherAvailabilityResponse(remote_device_));

  // The observer is notified of the scanned device.
  DeviceStatus device_status = CreateFakeDeviceStatus();
  HostScannerOperation::ScannedDeviceInfo scanned_device(
      remote_device_, device_status, false /* setup_required */);
  std::vector<HostScannerOperation::ScannedDeviceInfo> scanned_devices(
      {scanned_device});
  EXPECT_CALL(mock_observer_,
              OnTetherAvailabilityResponse(scanned_devices,
                                           multidevice::RemoteDeviceRefList(),
                                           false /* is_final_scan_result */));

  // Respond with TETHER_AVAILABLE response code and the device info and status.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE);
  response.mutable_device_status()->CopyFrom(device_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message), remote_device_);

  // Connection is preserved.
  EXPECT_EQ(remote_device_.GetDeviceId(),
            fake_connection_preserver_
                .last_requested_preserved_connection_device_id());
}

TEST_F(HostScannerOperationTest, LastProvisioningFailed) {
  // The scanned device is recorded.
  EXPECT_CALL(mock_tether_host_response_recorder_,
              RecordSuccessfulTetherAvailabilityResponse(remote_device_));

  // The observer is notified of the scanned device.
  DeviceStatus device_status = CreateFakeDeviceStatus();
  HostScannerOperation::ScannedDeviceInfo scanned_device(
      remote_device_, device_status, false /* setup_required */);
  std::vector<HostScannerOperation::ScannedDeviceInfo> scanned_devices(
      {scanned_device});
  EXPECT_CALL(mock_observer_,
              OnTetherAvailabilityResponse(scanned_devices,
                                           multidevice::RemoteDeviceRefList(),
                                           false /* is_final_scan_result */));

  // Respond with TETHER_AVAILABLE response code and the device info and status.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_LAST_PROVISIONING_FAILED);
  response.mutable_device_status()->CopyFrom(device_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message), remote_device_);

  // Connection is preserved.
  EXPECT_EQ(remote_device_.GetDeviceId(),
            fake_connection_preserver_
                .last_requested_preserved_connection_device_id());
}

TEST_F(HostScannerOperationTest, SetupRequired) {
  // The scanned device is recorded.
  EXPECT_CALL(mock_tether_host_response_recorder_,
              RecordSuccessfulTetherAvailabilityResponse(remote_device_));

  // The observer is notified that the scanned device has the |setup_required|
  // flag set.
  DeviceStatus device_status = CreateFakeDeviceStatus();
  HostScannerOperation::ScannedDeviceInfo scanned_device(
      remote_device_, device_status, true /* setup_required */);
  std::vector<HostScannerOperation::ScannedDeviceInfo> scanned_devices(
      {scanned_device});
  EXPECT_CALL(mock_observer_,
              OnTetherAvailabilityResponse(scanned_devices,
                                           multidevice::RemoteDeviceRefList(),
                                           false /* is_final_scan_result */));

  // Respond with SETUP_NEEDED response code and the device info and status.
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED);
  response.mutable_device_status()->CopyFrom(device_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message), remote_device_);

  // Connection is preserved.
  EXPECT_EQ(remote_device_.GetDeviceId(),
            fake_connection_preserver_
                .last_requested_preserved_connection_device_id());
}

}  // namespace tether

}  // namespace chromeos
