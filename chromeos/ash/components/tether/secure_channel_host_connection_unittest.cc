// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/secure_channel_host_connection.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

namespace {

class FakeHostConnectionPayloadListener
    : public HostConnection::PayloadListener {
 public:
  ~FakeHostConnectionPayloadListener() override = default;

  // HostConnection::PayloadListener:
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message) override {
    received_messages_.push_back(std::move(message));
  }

  std::vector<std::unique_ptr<MessageWrapper>> received_messages_;
};

}  // namespace

class SecureChannelHostConnectionTest : public testing::Test {
 public:
  SecureChannelHostConnectionTest(const SecureChannelHostConnectionTest&) =
      delete;
  SecureChannelHostConnectionTest& operator=(
      const SecureChannelHostConnectionTest&) = delete;

  void OnSend() { message_sent_ = true; }

 protected:
  SecureChannelHostConnectionTest()
      : fake_local_device_(multidevice::RemoteDeviceRefBuilder()
                               .SetPublicKey("local device")
                               .Build()),
        fake_remote_device_(multidevice::RemoteDeviceRefBuilder()
                                .SetPublicKey("remote device")
                                .Build()) {}

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(fake_local_device_);
    fake_device_sync_client_->set_synced_devices(
        multidevice::RemoteDeviceRefList{fake_remote_device_});
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_tether_host_fetcher_ =
        std::make_unique<FakeTetherHostFetcher>(fake_remote_device_);
    fake_host_payload_listener_ =
        std::make_unique<FakeHostConnectionPayloadListener>();
    host_connection_factory_ =
        std::make_unique<SecureChannelHostConnection::Factory>(
            fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
            fake_tether_host_fetcher_.get());
  }

  void SetupNextConnectionAttempt(
      secure_channel::FakeConnectionAttempt* connection_attempt) {
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        fake_remote_device_, fake_local_device_,
        base::WrapUnique<secure_channel::ConnectionAttempt>(
            connection_attempt));
  }

  bool message_sent_ = false;
  const multidevice::RemoteDeviceRef fake_local_device_;
  const multidevice::RemoteDeviceRef fake_remote_device_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<FakeHostConnectionPayloadListener>
      fake_host_payload_listener_;
  std::unique_ptr<SecureChannelHostConnection::Factory>
      host_connection_factory_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<SecureChannelHostConnectionTest> weak_ptr_factory_{this};
};

TEST_F(SecureChannelHostConnectionTest, TestDoesCreateConnectionByDeviceId) {
  // Create a connection attempt.
  auto* fake_connection_attempt = new secure_channel::FakeConnectionAttempt();
  SetupNextConnectionAttempt(fake_connection_attempt);

  base::test::TestFuture<std::unique_ptr<HostConnection>> future;

  // Create a host connection by device ID.
  host_connection_factory_->ScanForTetherHostAndCreateConnection(
      fake_remote_device_.GetDeviceId(),
      HostConnection::Factory::ConnectionPriority::kLow,
      fake_host_payload_listener_.get(), base::DoNothing(),
      future.GetCallback());

  // Finish the connection attempt.
  fake_connection_attempt->NotifyConnection(
      std::make_unique<secure_channel::FakeClientChannel>());

  // Expect the connection is now connected.
  EXPECT_TRUE(future.Get());
}

TEST_F(SecureChannelHostConnectionTest,
       TestDoesFailWhenConnectionAttemptFails) {
  // Create a connection attempt.
  auto* fake_connection_attempt = new secure_channel::FakeConnectionAttempt();
  SetupNextConnectionAttempt(fake_connection_attempt);

  base::test::TestFuture<std::unique_ptr<HostConnection>> future;

  // Create a host connection by device ID.
  host_connection_factory_->ScanForTetherHostAndCreateConnection(
      fake_remote_device_.GetDeviceId(),
      HostConnection::Factory::ConnectionPriority::kLow,
      fake_host_payload_listener_.get(), base::DoNothing(),
      future.GetCallback());

  // Finish the connection attempt.
  fake_connection_attempt->NotifyConnectionAttemptFailure(
      ash::secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR);

  // Expect the connection is now connected.
  EXPECT_FALSE(future.Get());
}

TEST_F(SecureChannelHostConnectionTest, TestDoesSendMessage) {
  // Create a connection attempt.
  auto* fake_connection_attempt = new secure_channel::FakeConnectionAttempt();
  SetupNextConnectionAttempt(fake_connection_attempt);

  // Setup client channel.
  secure_channel::FakeClientChannel* fake_client_channel =
      new secure_channel::FakeClientChannel();

  base::test::TestFuture<std::unique_ptr<HostConnection>> future;

  // Create the connection.
  host_connection_factory_->Create(
      TetherHost(fake_remote_device_),
      HostConnection::Factory::ConnectionPriority::kLow,
      fake_host_payload_listener_.get(), base::DoNothing(),
      future.GetCallback());

  // Finish the connection.
  fake_connection_attempt->NotifyConnection(
      base::WrapUnique(fake_client_channel));

  // Send the message.
  auto message = std::make_unique<MessageWrapper>(TetherAvailabilityResponse());
  std::string expected_payload = message->ToRawMessage();
  future.Get()->SendMessage(
      std::move(message),
      base::BindOnce(&SecureChannelHostConnectionTest::OnSend,
                     weak_ptr_factory_.GetWeakPtr()));

  base::RunLoop().RunUntilIdle();

  // Expect the message was sent.
  std::vector<std::pair<std::string, base::OnceClosure>>& sent_messages =
      fake_client_channel->sent_messages();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first);

  // Fire the callback - expect the delegate is notified of the message send.
  std::move(sent_messages[0].second).Run();
  EXPECT_TRUE(message_sent_);
}

TEST_F(SecureChannelHostConnectionTest, TestDoesReceiveMessage) {
  // Create a connection attempt.
  auto* fake_connection_attempt = new secure_channel::FakeConnectionAttempt();
  SetupNextConnectionAttempt(fake_connection_attempt);

  // Setup client channel.
  secure_channel::FakeClientChannel* fake_client_channel =
      new secure_channel::FakeClientChannel();

  base::test::TestFuture<std::unique_ptr<HostConnection>> future;

  // Create the connection.
  host_connection_factory_->Create(
      TetherHost(fake_remote_device_),
      HostConnection::Factory::ConnectionPriority::kLow,
      fake_host_payload_listener_.get(), base::DoNothing(),
      future.GetCallback());

  // Finish the connection.
  fake_connection_attempt->NotifyConnection(
      base::WrapUnique(fake_client_channel));

  EXPECT_TRUE(future.Wait());

  std::unique_ptr<MessageWrapper> message =
      std::make_unique<MessageWrapper>(TetherAvailabilityResponse());
  fake_client_channel->NotifyMessageReceived(message->ToRawMessage());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, fake_host_payload_listener_->received_messages_.size());
  EXPECT_EQ(message->ToRawMessage(),
            fake_host_payload_listener_->received_messages_[0]->ToRawMessage());
}

TEST_F(SecureChannelHostConnectionTest, TestDoesSendDisconnection) {
  // Create a connection attempt.
  auto* fake_connection_attempt = new secure_channel::FakeConnectionAttempt();
  SetupNextConnectionAttempt(fake_connection_attempt);

  // Setup client channel.
  secure_channel::FakeClientChannel* fake_client_channel =
      new secure_channel::FakeClientChannel();

  base::test::TestFuture<std::unique_ptr<HostConnection>> future;
  base::test::TestFuture<void> disconnection_callback;

  // Create the connection.
  host_connection_factory_->Create(
      TetherHost(fake_remote_device_),
      HostConnection::Factory::ConnectionPriority::kLow,
      fake_host_payload_listener_.get(), disconnection_callback.GetCallback(),
      future.GetCallback());

  // Finish the connection.
  fake_connection_attempt->NotifyConnection(
      base::WrapUnique(fake_client_channel));

  EXPECT_TRUE(future.Wait());

  fake_client_channel->NotifyDisconnected();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(disconnection_callback.Wait());
}
}  // namespace ash::tether
