// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/connection_manager_impl.h"

#include <memory>

#include "base/macros.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

using multidevice_setup::mojom::HostStatus;

class FakeObserver : public ConnectionManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t status_num_calls() const { return status_changed_num_calls_; }
  size_t message_cb_num_calls() const { return message_received_num_calls_; }

  const std::string& last_message() const { return last_message_; }

  // ConnectionManager::Observer:
  void OnConnectionStatusChanged() override { ++status_changed_num_calls_; }
  void OnMessageReceived(const std::string& payload) override {
    last_message_ = payload;
    ++message_received_num_calls_;
  }

 private:
  size_t status_changed_num_calls_ = 0;
  size_t message_received_num_calls_ = 0;
  std::string last_message_;
};

}  // namespace

class ConnectionManagerImplTest : public testing::Test {
 protected:
  ConnectionManagerImplTest()
      : test_remote_device_(
            chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        test_local_device_(
            chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        fake_secure_channel_client_(
            std::make_unique<
                chromeos::secure_channel::FakeSecureChannelClient>()) {}

  ConnectionManagerImplTest(const ConnectionManagerImplTest&) = delete;
  ConnectionManagerImplTest& operator=(const ConnectionManagerImplTest&) =
      delete;
  ~ConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_device_sync_client_.set_local_device_metadata(test_local_device_);
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(HostStatus::kHostVerified, test_remote_device_));
    connection_manager_ = std::make_unique<ConnectionManagerImpl>(
        &fake_multidevice_setup_client_, &fake_device_sync_client_,
        fake_secure_channel_client_.get());
    connection_manager_->AddObserver(&fake_observer_);
    EXPECT_EQ(ConnectionManager::Status::kDisconnected, GetStatus());
  }

  void TearDown() override {
    connection_manager_->RemoveObserver(&fake_observer_);
  }

  ConnectionManager::Status GetStatus() const {
    return connection_manager_->GetStatus();
  }

  size_t GetNumStatusObserverCalls() const {
    return fake_observer_.status_num_calls();
  }

  size_t GetNumMessageReceivedObserverCalls() const {
    return fake_observer_.message_cb_num_calls();
  }

  void CreateFakeConnectionAttempt() {
    auto fake_connection_attempt =
        std::make_unique<chromeos::secure_channel::FakeConnectionAttempt>();
    fake_connection_attempt_ = fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_initiate_connection_attempt(
        test_remote_device_, test_local_device_,
        std::move(fake_connection_attempt));
  }

  chromeos::multidevice::RemoteDeviceRef test_remote_device_;
  chromeos::multidevice::RemoteDeviceRef test_local_device_;
  device_sync::FakeDeviceSyncClient fake_device_sync_client_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  std::unique_ptr<chromeos::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<ConnectionManagerImpl> connection_manager_;
  FakeObserver fake_observer_;
  chromeos::secure_channel::FakeConnectionAttempt* fake_connection_attempt_;
};

TEST_F(ConnectionManagerImplTest, SuccessfullyAttemptConnection) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kConnecting, GetStatus());

  auto fake_client_channel =
      std::make_unique<chromeos::secure_channel::FakeClientChannel>();
  fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Status has been updated to connected, verify that the status observer has
  // been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kConnected, GetStatus());
}

TEST_F(ConnectionManagerImplTest, FailedToAttemptConnection) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kConnecting, GetStatus());

  fake_connection_attempt_->NotifyConnectionAttemptFailure(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR);

  // Status has been updated to disconnected, verify that the status observer
  // has been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kDisconnected, GetStatus());
}

TEST_F(ConnectionManagerImplTest, SuccessfulAttemptConnectionButDisconnected) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kConnecting, GetStatus());

  auto fake_client_channel =
      std::make_unique<chromeos::secure_channel::FakeClientChannel>();
  chromeos::secure_channel::FakeClientChannel* fake_client_channel_raw =
      fake_client_channel.get();
  fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Status has been updated to connected, verify that the status observer has
  // been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kConnected, GetStatus());

  // Simulate a disconnected channel.
  fake_client_channel_raw->NotifyDisconnected();

  // Expect status to be updated to disconnected.
  EXPECT_EQ(3u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kDisconnected, GetStatus());
}

TEST_F(ConnectionManagerImplTest, AttemptConnectionWithMessageReceived) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kConnecting, GetStatus());

  auto fake_client_channel =
      std::make_unique<chromeos::secure_channel::FakeClientChannel>();
  chromeos::secure_channel::FakeClientChannel* fake_client_channel_raw =
      fake_client_channel.get();
  fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Status has been updated to connected, verify that the status observer has
  // been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kConnected, GetStatus());

  // Simulate a message being sent.
  const std::string expected_payload = "payload";
  fake_client_channel_raw->NotifyMessageReceived(expected_payload);

  // Expected MessageReceived() callback to be called.
  EXPECT_EQ(1u, GetNumMessageReceivedObserverCalls());
  EXPECT_EQ(expected_payload, fake_observer_.last_message());
}

TEST_F(ConnectionManagerImplTest, AttemptConnectionWithoutLocalDevice) {
  // Simulate a missing local device.
  fake_device_sync_client_.set_local_device_metadata(
      base::Optional<chromeos::multidevice::RemoteDeviceRef>());
  connection_manager_->AttemptConnection();

  // Status is still disconnected since there is a missing device, verify that
  // the status observer did not get called (exited early).
  EXPECT_EQ(0u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kDisconnected, GetStatus());
}

TEST_F(ConnectionManagerImplTest, AttemptConnectionWithoutRemoteDevice) {
  // Simulate a missing remote device.
  fake_multidevice_setup_client_.SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified,
                     base::Optional<chromeos::multidevice::RemoteDeviceRef>()));
  connection_manager_->AttemptConnection();

  // Status is still disconnected since there is a missing device, verify that
  // the status observer did not get called (exited early).
  EXPECT_EQ(0u, GetNumStatusObserverCalls());
  EXPECT_EQ(ConnectionManager::Status::kDisconnected, GetStatus());
}

}  // namespace phonehub
}  // namespace chromeos
