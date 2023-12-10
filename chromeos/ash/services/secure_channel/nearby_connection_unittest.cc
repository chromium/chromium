// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_connection.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/connection_observer.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const char kTestBluetoothAddress[] = "01:23:45:67:89:AB";

// Returns the same address as above except as a byte vector.
const std::vector<uint8_t>& GetTestBluetoothAddressAsVector() {
  static const std::vector<uint8_t> address{0x01, 0x23, 0x45, 0x67, 0x89, 0xab};
  return address;
}

const std::vector<uint8_t> GetEid() {
  return std::vector<uint8_t>{0, 1};
}

multidevice::RemoteDeviceRef CreateTestDevice() {
  multidevice::RemoteDeviceRef device =
      multidevice::CreateRemoteDeviceRefForTest();
  multidevice::GetMutableRemoteDevice(device)->bluetooth_public_address =
      kTestBluetoothAddress;
  return device;
}

void ExpectFileTransferUpdate(const mojom::FileTransferUpdate* update,
                              mojom::FileTransferStatus status,
                              uint64_t total_bytes,
                              uint64_t bytes_transferred) {
  EXPECT_EQ(status, update->status);
  EXPECT_EQ(total_bytes, update->total_bytes);
  EXPECT_EQ(bytes_transferred, update->bytes_transferred);
}

class FakeConnectionObserver : public ConnectionObserver {
 public:
  FakeConnectionObserver() = default;
  ~FakeConnectionObserver() override = default;

  Connection::Status last_status_update = Connection::Status::DISCONNECTED;
  std::optional<WireMessage> last_received_message;
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
    connection_ = NearbyConnection::Factory::Create(test_device_, GetEid(),
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

  void RegisterPayloadFile(
      int64_t payload_id,
      const FileTransferUpdateCallback& file_transfer_update_callback) {
    base::FilePath file_path;
    base::CreateTemporaryFile(&file_path);
    base::File input_file(
        file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    base::File output_file(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                          base::File::Flags::FLAG_WRITE);

    base::RunLoop register_run_loop;
    connection()->RegisterPayloadFile(
        payload_id,
        mojom::PayloadFiles::New(std::move(input_file), std::move(output_file)),
        std::move(file_transfer_update_callback),
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          register_run_loop.Quit();
        }));
    register_run_loop.Run();
  }

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

TEST_F(SecureChannelNearbyConnectionTest, MultipleFileTransferUpdates) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Register payload file.
  std::vector<mojom::FileTransferUpdatePtr> file_transfer_updates;
  base::RunLoop transfer_run_loop;
  RegisterPayloadFile(
      /*payload_Id=*/1234,
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        EXPECT_EQ(1234, update->payload_id);
        if (update->status != mojom::FileTransferStatus::kInProgress) {
          transfer_run_loop.Quit();
        }
        file_transfer_updates.push_back(std::move(update));
      }));
  EXPECT_TRUE(fake_connection->register_payload_file_requests().contains(1234));

  // Send file transfer updates.
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/1000, /*bytes_transferred=*/100);
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/1000, /*bytes_transferred=*/500);
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  transfer_run_loop.Run();
  EXPECT_EQ(3ul, file_transfer_updates.size());
  ExpectFileTransferUpdate(file_transfer_updates.at(0).get(),
                           mojom::FileTransferStatus::kInProgress,
                           /*total_bytes=*/1000, /*bytes_transferred=*/100);
  ExpectFileTransferUpdate(file_transfer_updates.at(1).get(),
                           mojom::FileTransferStatus::kInProgress,
                           /*total_bytes=*/1000, /*bytes_transferred=*/500);
  ExpectFileTransferUpdate(file_transfer_updates.at(2).get(),
                           mojom::FileTransferStatus::kSuccess,
                           /*total_bytes=*/1000, /*bytes_transferred=*/1000);

  // Disconnect.
  base::RunLoop disconnect_run_loop;
  fake_observer_.on_status_change_closure = disconnect_run_loop.QuitClosure();
  connection()->Disconnect();
  disconnect_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());
}

TEST_F(SecureChannelNearbyConnectionTest, RegisterMultiplePayloadFiles) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Register payload files.
  std::vector<mojom::FileTransferUpdatePtr> first_payload_transfer_updates;
  std::vector<mojom::FileTransferUpdatePtr> second_payload_transfer_updates;
  base::RunLoop first_payload_transfer_run_loop;
  base::RunLoop second_payload_transfer_run_loop;

  RegisterPayloadFile(
      /*payload_Id=*/1234,
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        EXPECT_EQ(1234, update->payload_id);
        if (update->status != mojom::FileTransferStatus::kInProgress) {
          first_payload_transfer_run_loop.Quit();
        }
        first_payload_transfer_updates.push_back(std::move(update));
      }));
  RegisterPayloadFile(
      /*payload_Id=*/-5678,
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        EXPECT_EQ(-5678, update->payload_id);
        if (update->status != mojom::FileTransferStatus::kInProgress) {
          second_payload_transfer_run_loop.Quit();
        }
        second_payload_transfer_updates.push_back(std::move(update));
      }));
  EXPECT_TRUE(fake_connection->register_payload_file_requests().contains(1234));
  EXPECT_TRUE(
      fake_connection->register_payload_file_requests().contains(-5678));

  // Send file transfer updates.
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/-5678, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/2000, /*bytes_transferred=*/2000);
  first_payload_transfer_run_loop.Run();
  second_payload_transfer_run_loop.Run();
  EXPECT_EQ(1ul, first_payload_transfer_updates.size());
  ExpectFileTransferUpdate(first_payload_transfer_updates.at(0).get(),
                           mojom::FileTransferStatus::kSuccess,
                           /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  EXPECT_EQ(1ul, second_payload_transfer_updates.size());
  ExpectFileTransferUpdate(second_payload_transfer_updates.at(0).get(),
                           mojom::FileTransferStatus::kSuccess,
                           /*total_bytes=*/2000, /*bytes_transferred=*/2000);

  // Disconnect.
  base::RunLoop disconnect_run_loop;
  fake_observer_.on_status_change_closure = disconnect_run_loop.QuitClosure();
  connection()->Disconnect();
  disconnect_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());
}

TEST_F(SecureChannelNearbyConnectionTest,
       RemoteDisconnectsBeforeTransferComplete) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Register payload files.
  std::vector<mojom::FileTransferUpdatePtr> first_payload_transfer_updates;
  std::vector<mojom::FileTransferUpdatePtr> second_payload_transfer_updates;
  base::RunLoop first_payload_transfer_run_loop;
  base::RunLoop second_payload_transfer_run_loop;

  RegisterPayloadFile(
      /*payload_Id=*/1234,
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        if (update->status != mojom::FileTransferStatus::kInProgress) {
          first_payload_transfer_run_loop.Quit();
        }
        first_payload_transfer_updates.push_back(std::move(update));
      }));
  RegisterPayloadFile(
      /*payload_Id=*/-5678,
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        if (update->status != mojom::FileTransferStatus::kInProgress) {
          second_payload_transfer_run_loop.Quit();
        }
        second_payload_transfer_updates.push_back(std::move(update));
      }));

  // Send file transfer updates.
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/-5678, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/2000, /*bytes_transferred=*/500);
  first_payload_transfer_run_loop.Run();

  // Disconnect the FilePayloadListener remotes before the second payload
  // transfer is complete, and wait for the receiver to get the disconnect
  // notification and send out the cancelation update.
  fake_connection->DisconnectPendingFileTransfers();
  second_payload_transfer_run_loop.Run();

  // Now disconnect the rest of the connection.
  base::RunLoop disconnect_run_loop;
  fake_observer_.on_status_change_closure = disconnect_run_loop.QuitClosure();
  fake_connection->Disconnect();
  disconnect_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());

  EXPECT_EQ(1ul, first_payload_transfer_updates.size());
  ExpectFileTransferUpdate(first_payload_transfer_updates.at(0).get(),
                           mojom::FileTransferStatus::kSuccess,
                           /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  // Only incomplete transfers should get the additional cancelation update.
  EXPECT_EQ(2ul, second_payload_transfer_updates.size());
  ExpectFileTransferUpdate(second_payload_transfer_updates.at(0).get(),
                           mojom::FileTransferStatus::kInProgress,
                           /*total_bytes=*/2000, /*bytes_transferred=*/500);
  ExpectFileTransferUpdate(second_payload_transfer_updates.at(1).get(),
                           mojom::FileTransferStatus::kCanceled,
                           /*total_bytes=*/0, /*bytes_transferred=*/0);
}

TEST_F(SecureChannelNearbyConnectionTest,
       ClientDisconnectsBeforeTransferComplete) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Register payload files.
  std::vector<mojom::FileTransferUpdatePtr> file_transfer_updates;
  base::RunLoop in_progress_update_run_loop;
  base::RunLoop canceled_update_run_loop;

  RegisterPayloadFile(
      /*payload_Id=*/1234,
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        if (update->status == mojom::FileTransferStatus::kInProgress) {
          in_progress_update_run_loop.Quit();
        } else if (update->status == mojom::FileTransferStatus::kCanceled) {
          canceled_update_run_loop.Quit();
        }
        file_transfer_updates.push_back(std::move(update));
      }));

  // Send file transfer updates.
  fake_connection->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/1000, /*bytes_transferred=*/100);
  in_progress_update_run_loop.Run();

  // Disconnect from the client connection.
  connection()->Disconnect();
  canceled_update_run_loop.Run();
  EXPECT_EQ(Connection::Status::DISCONNECTED,
            fake_observer_.last_status_update);
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection()->status());

  EXPECT_EQ(2ul, file_transfer_updates.size());
  ExpectFileTransferUpdate(file_transfer_updates.at(0).get(),
                           mojom::FileTransferStatus::kInProgress,
                           /*total_bytes=*/1000, /*bytes_transferred=*/100);
  ExpectFileTransferUpdate(file_transfer_updates.at(1).get(),
                           mojom::FileTransferStatus::kCanceled,
                           /*total_bytes=*/0, /*bytes_transferred=*/0);
}

TEST_F(SecureChannelNearbyConnectionTest,
       DisconnectsAfterReceivingUnregisteredPayloadUpdate) {
  FakeNearbyConnector::FakeConnection* fake_connection = CompleteConnection();

  // Register payload files.
  base::RunLoop canceled_update_run_loop;
  RegisterPayloadFile(
      /*payload_Id=*/1234,
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        ExpectFileTransferUpdate(update.get(),
                                 mojom::FileTransferStatus::kCanceled,
                                 /*total_bytes=*/0, /*bytes_transferred=*/0);
        canceled_update_run_loop.Quit();
      }));

  // Send file transfer update with an unregistered payload ID.
  fake_connection->SendUnexpectedFileTransferUpdate(
      /*unexpected_payload_id=*/-5678);
  canceled_update_run_loop.Run();

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

}  // namespace ash::secure_channel
