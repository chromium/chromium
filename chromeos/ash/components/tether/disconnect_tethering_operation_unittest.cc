// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/disconnect_tethering_operation.h"

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"
#include "chromeos/ash/components/tether/test_timer_factory.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

constexpr base::TimeDelta kDisconnectTetheringRequestTime = base::Seconds(3);

// Used to verify the DisonnectTetheringOperation notifies the observer when
// appropriate.
class MockOperationObserver : public DisconnectTetheringOperation::Observer {
 public:
  MockOperationObserver() = default;

  MockOperationObserver(const MockOperationObserver&) = delete;
  MockOperationObserver& operator=(const MockOperationObserver&) = delete;

  ~MockOperationObserver() = default;

  MOCK_METHOD2(OnOperationFinished, void(const std::string&, bool));
};

}  // namespace

class DisconnectTetheringOperationTest : public testing::Test {
 public:
  DisconnectTetheringOperationTest(const DisconnectTetheringOperationTest&) =
      delete;
  DisconnectTetheringOperationTest& operator=(
      const DisconnectTetheringOperationTest&) = delete;

 protected:
  DisconnectTetheringOperationTest()
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

  std::unique_ptr<DisconnectTetheringOperation> ConstructOperation() {
    auto connection_attempt =
        std::make_unique<secure_channel::FakeConnectionAttempt>();
    connection_attempt_ = connection_attempt.get();
    // remote_device_to_fake_connection_attempt_map_[remote_device_] =
    //     fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        remote_device_, local_device_, std::move(connection_attempt));

    auto operation = base::WrapUnique(new DisconnectTetheringOperation(
        remote_device_, fake_device_sync_client_.get(),
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

  raw_ptr<secure_channel::FakeConnectionAttempt, ExperimentalAsh>
      connection_attempt_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;

  base::SimpleTestClock test_clock_;
  MockOperationObserver mock_observer_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<DisconnectTetheringOperation> operation_;
};

TEST_F(DisconnectTetheringOperationTest, TestSuccess) {
  // Verify that the Observer is called with success and the correct parameters.
  EXPECT_CALL(mock_observer_, OnOperationFinished(remote_device_.GetDeviceId(),
                                                  true /* successful */));

  // Advance the clock in order to verify a non-zero request duration is
  // recorded and verified (below).
  test_clock_.Advance(kDisconnectTetheringRequestTime);

  // Execute the operation.
  operation_->OnMessageSent(0 /* sequence_number */);

  // Verify the request duration metric is recorded.
  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.DisconnectTetheringRequestDuration",
      kDisconnectTetheringRequestTime, 1);
}

TEST_F(DisconnectTetheringOperationTest, TestFailure) {
  // Verify that the observer is called with failure and the correct parameters.
  EXPECT_CALL(mock_observer_, OnOperationFinished(remote_device_.GetDeviceId(),
                                                  false /* successful */));

  // Finalize the operation; no message has been sent so this represents a
  // failure case.
  operation_->UnregisterDevice(remote_device_);

  histogram_tester_.ExpectTotalCount(
      "InstantTethering.Performance.DisconnectTetheringRequestDuration", 0);
}

// Tests that the DisonnectTetheringRequest message is sent to the remote device
// once the communication channel is connected and authenticated.
TEST_F(DisconnectTetheringOperationTest,
       DisconnectRequestSentOnceAuthenticated) {
  std::unique_ptr<DisconnectTetheringOperation> operation =
      ConstructOperation();
  operation->Initialize();

  // Create the client channel for the remote device.
  auto fake_client_channel =
      std::make_unique<secure_channel::FakeClientChannel>();

  // No requests as a result of creating the client channel.
  auto& sent_messages = fake_client_channel->sent_messages();
  EXPECT_EQ(0u, sent_messages.size());

  // Connect and authenticate the client channel.
  connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Verify the DisconnectTetheringRequest message is sent.
  auto message_wrapper =
      std::make_unique<MessageWrapper>(DisconnectTetheringRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first);
}

}  // namespace tether

}  // namespace ash
