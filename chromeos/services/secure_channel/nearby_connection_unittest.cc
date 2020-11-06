// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/nearby_connection.h"

#include "base/callback.h"
#include "base/test/task_environment.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/secure_channel/connection_observer.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_nearby_connector.h"
#include "chromeos/services/secure_channel/wire_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace secure_channel {
namespace {

const char kTestBluetoothAddress[] = "01:23:45:67:89:AB";

// Returns the same address as above except as a byte vector.
const std::vector<uint8_t>& GetTestBluetoothAddressAsVector() {
  static const std::vector<uint8_t> address{0x01, 0x23, 0x45, 0x67, 0x89, 0xab};
  return address;
}

multidevice::RemoteDeviceRef CreateTestDevice() {
  multidevice::RemoteDeviceRef device =
      multidevice::CreateRemoteDeviceRefForTest();
  multidevice::GetMutableRemoteDevice(device)->bluetooth_public_address =
      kTestBluetoothAddress;
  return device;
}

class FakeConnectionObserver : public ConnectionObserver {
 public:
  FakeConnectionObserver() = default;
  ~FakeConnectionObserver() override = default;

  Connection::Status last_status_update = Connection::Status::DISCONNECTED;
  base::Optional<WireMessage> last_received_message;
  bool last_send_complete_success = false;

  base::OnceClosure on_status_change_closure;
  base::OnceClosure on_message_received_closure;
  base::OnceClosure on_send_complete_closure;

 private:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override {
    last_status_update = new_status;

    if (on_status_change_closure)
      std::move(on_status_change_closure).Run();
  }

  void OnMessageReceived(const Connection& connection,
                         const WireMessage& message) override {
    last_received_message = message;
    std::move(on_message_received_closure).Run();
  }

  void OnSendCompleted(const Connection& connection,
                       const WireMessage& message,
                       bool success) override {
    last_send_complete_success = success;
    std::move(on_send_complete_closure).Run();
  }
};

}  // namespace

class SecureChannelNearbyConnectionTest : public testing::Test {
 protected:
  SecureChannelNearbyConnectionTest() = default;
  ~SecureChannelNearbyConnectionTest() override = default;

  void SetUp() override {
    connection_ = NearbyConnection::Factory::Create(test_device_,
                                                    &fake_nearby_connector_);
    connection_->AddObserver(&fake_observer_);

    base::RunLoop start_connect_run_loop;
    fake_nearby_connector_.on_connect_closure =
        start_connect_run_loop.QuitClosure();
    EXPECT_EQ(Connection::Status::DISCONNECTED, connection_->status());
    connection_->Connect();
    start_connect_run_loop.Run();
    EXPECT_EQ(Connection::Status::IN_PROGRESS, connection_->status());
    EXPECT_EQ(Connection::Status::IN_PROGRESS,
              fake_observer_.last_status_update);
  }

  void TearDown() override { connection_->RemoveObserver(&fake_observer_); }

  FakeNearbyConnector::FakeConnection* CompleteConnection() {
    base::RunLoop connect_run_loop;
    fake_observer_.on_status_change_closure = connect_run_loop.QuitClosure();
    FakeNearbyConnector::FakeConnection* fake_connection =
        fake_nearby_connector_.ConnectQueuedCallback();
    connect_run_loop.Run();

    EXPECT_EQ(GetTestBluetoothAddressAsVector(),
              fake_connection->bluetooth_public_address());
    EXPECT_EQ(Connection::Status::CONNECTED, fake_observer_.last_status_update);
    return fake_connection;
  }

  Connection* connection() { return connection_.get(); }

  FakeNearbyConnector fake_nearby_connector_;
  FakeConnectionObserver fake_observer_;

 private:
  base::test::TaskEnvironment task_environment_;
  multidevice::RemoteDeviceRef test_device_ = CreateTestDevice();

  std::unique_ptr<Connection> connection_;
};

TEST_F(SecureChannelNearbyConnectionTest, ConnectAndTransferMessages) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Send message.
  WireMessage message_to_send("send_payload", "send_feature");
  base::RunLoop send_run_loop;
  fake_observer_.on_send_complete_closure = send_run_loop.QuitClosure();
  connection()->SendMessage(std::make_unique<WireMessage>(message_to_send));
  send_run_loop.Run();
  EXPECT_EQ(message_to_send.Serialize(), fake_connection->sent_messages()[0]);

  // Receive message.
  WireMessage message_to_receive("receive_payload", "receive_feature");
  base::RunLoop receive_run_loop;
  fake_observer_.on_message_received_closure = receive_run_loop.QuitClosure();
  fake_connection->ReceiveMessage(message_to_receive.Serialize());
  receive_run_loop.Run();
  EXPECT_EQ("receive_payload", fake_observer_.last_received_message->payload());
  EXPECT_EQ("receive_feature", fake_observer_.last_received_message->feature());

  // Disconnect.
  base::RunLoop disconnect_run_loop;
  fake_observer_.on_status_change_closure = disconnect_run_loop.QuitClosure();
  connection()->Disconnect();
  disconnect_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());
}

TEST_F(SecureChannelNearbyConnectionTest, FailToSendMessage) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Simulate an error sending a message; NearbyConnection should treat this as
  // a fatal error and disconnect.
  WireMessage message_to_send("payload", "feature");
  base::RunLoop send_run_loop;
  fake_observer_.on_send_complete_closure = send_run_loop.QuitClosure();
  fake_connection->set_should_send_succeed(false);
  connection()->SendMessage(std::make_unique<WireMessage>(message_to_send));
  send_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());
}

TEST_F(SecureChannelNearbyConnectionTest, FailToConnect) {
  base::RunLoop status_change_run_loop;
  fake_observer_.on_status_change_closure =
      status_change_run_loop.QuitClosure();
  fake_nearby_connector_.FailQueuedCallback();
  status_change_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());
}

TEST_F(SecureChannelNearbyConnectionTest, DisconnectFromRemoteDevice) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Simulate the remote device disconnecting and verify that NearbyConnection
  // changes its status once the Mojo connection is dropped.
  base::RunLoop disconnect_run_loop;
  fake_observer_.on_status_change_closure = disconnect_run_loop.QuitClosure();
  fake_connection->Disconnect();
  disconnect_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());
}

}  // namespace secure_channel
}  // namespace chromeos
