// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/keep_alive_operation.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/components/tether/test_timer_factory.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NotNull;

namespace chromeos {

namespace tether {

namespace {

// Used to verify the KeepAliveOperation notifies the observer when appropriate.
class MockOperationObserver : public KeepAliveOperation::Observer {
 public:
  MockOperationObserver() = default;
  ~MockOperationObserver() = default;

  MOCK_METHOD2(OnOperationFinishedRaw,
               void(multidevice::RemoteDeviceRef, DeviceStatus*));

  void OnOperationFinished(multidevice::RemoteDeviceRef remote_device,
                           std::unique_ptr<DeviceStatus> device_status) {
    OnOperationFinishedRaw(remote_device, device_status.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOperationObserver);
};

}  // namespace

class KeepAliveOperationTest : public testing::Test {
 protected:
  KeepAliveOperationTest()
      : local_device_(multidevice::RemoteDeviceRefBuilder()
                          .SetPublicKey("local device")
                          .Build()),
        remote_device_(multidevice::CreateRemoteDeviceRefListForTest(1)[0]) {}

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(local_device_);
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    operation_ = ConstructOperation();
    operation_->Initialize();

    ConnectAuthenticatedChannelForDevice(remote_device_);
  }

  std::unique_ptr<KeepAliveOperation> ConstructOperation() {
    auto connection_attempt =
        std::make_unique<secure_channel::FakeConnectionAttempt>();
    connection_attempt_ = connection_attempt.get();
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        remote_device_, local_device_, std::move(connection_attempt));

    auto operation = base::WrapUnique(
        new KeepAliveOperation(remote_device_, fake_device_sync_client_.get(),
                               fake_secure_channel_client_.get()));
    operation->AddObserver(&mock_observer_);

    // Prepare the disconnection timeout timer to be made for the remote device.
    auto test_timer_factory = std::make_unique<TestTimerFactory>();
    test_timer_factory->set_device_id_for_next_timer(
        remote_device_.GetDeviceId());
    operation->SetTimerFactoryForTest(std::move(test_timer_factory));

    test_clock_.SetNow(base::Time::UnixEpoch());
    operation->SetClockForTest(&test_clock_);

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
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;

  std::unique_ptr<KeepAliveOperation> operation_;

  base::SimpleTestClock test_clock_;
  MockOperationObserver mock_observer_;
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeepAliveOperationTest);
};

// Tests that the KeepAliveTickle message is sent to the remote device once the
// communication channel is connected and authenticated.
TEST_F(KeepAliveOperationTest, KeepAliveTickleSentOnceAuthenticated) {
  std::unique_ptr<KeepAliveOperation> operation = ConstructOperation();
  operation->Initialize();

  // Create the client channel for the remote device.
  auto fake_client_channel =
      std::make_unique<secure_channel::FakeClientChannel>();

  // No requests as a result of creating the client channel.
  auto& sent_messages = fake_client_channel->sent_messages();
  EXPECT_EQ(0u, sent_messages.size());

  // Connect and authenticate the client channel.
  connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Verify the KeepAliveTickle message is sent.
  auto message_wrapper = std::make_unique<MessageWrapper>(KeepAliveTickle());
  std::string expected_payload = message_wrapper->ToRawMessage();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first);
}

// Tests that observers are notified when the operation has completed, signified
// by the OnMessageReceived handler being called.
TEST_F(KeepAliveOperationTest, NotifiesObserversOnResponse) {
  DeviceStatus test_status = CreateDeviceStatusWithFakeFields();

  // Verify that the observer is called with the correct parameters.
  EXPECT_CALL(mock_observer_, OnOperationFinishedRaw(remote_device_, NotNull()))
      .WillOnce(Invoke([this, &test_status](multidevice::RemoteDeviceRef device,
                                            DeviceStatus* status) {
        EXPECT_EQ(remote_device_, device);
        EXPECT_EQ(test_status.SerializeAsString(), status->SerializeAsString());
      }));

  KeepAliveTickleResponse response;
  response.mutable_device_status()->CopyFrom(test_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message), remote_device_);
}

TEST_F(KeepAliveOperationTest, RecordsResponseDuration) {
  static constexpr base::TimeDelta kKeepAliveTickleResponseTime =
      base::TimeDelta::FromSeconds(3);

  EXPECT_CALL(mock_observer_, OnOperationFinishedRaw(remote_device_, _));

  // Advance the clock in order to verify a non-zero response duration is
  // recorded and verified (below).
  test_clock_.Advance(kKeepAliveTickleResponseTime);

  std::unique_ptr<MessageWrapper> message(
      new MessageWrapper(KeepAliveTickleResponse()));
  operation_->OnMessageReceived(std::move(message), remote_device_);

  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.KeepAliveTickleResponseDuration",
      kKeepAliveTickleResponseTime, 1);
}

}  // namespace tether

}  // namespace chromeos
