// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel_impl.h"

#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/fake_channel.h"
#include "chromeos/ash/services/secure_channel/fake_secure_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel_observer.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

class SecureChannelClientChannelImplTest : public testing::Test {
 public:
  SecureChannelClientChannelImplTest(
      const SecureChannelClientChannelImplTest&) = delete;
  SecureChannelClientChannelImplTest& operator=(
      const SecureChannelClientChannelImplTest&) = delete;

 protected:
  SecureChannelClientChannelImplTest() = default;

  // testing::Test:
  void SetUp() override {
    fake_channel_ = std::make_unique<FakeChannel>();

    client_channel_ = ClientChannelImpl::Factory::Create(
        fake_channel_->GenerateRemote(),
        message_receiver_remote_.BindNewPipeAndPassReceiver(),
        nearby_connection_state_listener_remote_.BindNewPipeAndPassReceiver());

    fake_observer_ = std::make_unique<FakeClientChannelObserver>();
    client_channel_->AddObserver(fake_observer_.get());
  }

  void TearDown() override {
    if (client_channel_)
      client_channel_->RemoveObserver(fake_observer_.get());
  }

  mojom::ConnectionMetadataPtr CallGetConnectionMetadata() {
    EXPECT_FALSE(connection_metadata_);

    base::RunLoop run_loop;
    EXPECT_TRUE(client_channel_->GetConnectionMetadata(base::BindOnce(
        &SecureChannelClientChannelImplTest::OnGetConnectionMetadata,
        base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();

    return std::move(connection_metadata_);
  }

  int CallSendMessage(const std::string& message) {
    static int message_counter = 0;
    int counter_for_this_message = message_counter++;
    bool success = client_channel_->SendMessage(
        message,
        base::BindOnce(&SecureChannelClientChannelImplTest::OnMessageSent,
                       base::Unretained(this), counter_for_this_message));
    EXPECT_TRUE(success);
    SendPendingMojoMessages();
    return counter_for_this_message;
  }

  void CallSendMessageCallback(base::OnceClosure callback) {
    base::RunLoop run_loop;
    message_sent_callback_ = run_loop.QuitClosure();
    std::move(callback).Run();
    run_loop.Run();
  }

  void VerifyChannelDisconnected() {
    EXPECT_TRUE(client_channel_->is_disconnected());
    EXPECT_TRUE(fake_observer_->is_disconnected());

    // Ensure that these methods do not work once the ClientChannel is
    // disconnected.
    EXPECT_FALSE(client_channel_->GetConnectionMetadata(base::NullCallback()));
    EXPECT_FALSE(client_channel_->SendMessage("message", base::NullCallback()));
  }

  void SendPendingMojoMessages() {
    static_cast<ClientChannelImpl*>(client_channel_.get())->FlushForTesting();
  }

  mojom::PayloadFilesPtr CreatePayloadFiles() {
    base::FilePath file_path;
    base::CreateTemporaryFile(&file_path);
    base::File input_file(
        file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    base::File output_file(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                          base::File::Flags::FLAG_WRITE);
    return mojom::PayloadFiles::New(std::move(input_file),
                                    std::move(output_file));
  }

  void ExpectFileTransferUpdate(const mojom::FileTransferUpdate* update,
                                mojom::FileTransferStatus status,
                                uint64_t total_bytes,
                                uint64_t bytes_transferred) {
    EXPECT_EQ(status, update->status);
    EXPECT_EQ(total_bytes, update->total_bytes);
    EXPECT_EQ(bytes_transferred, update->bytes_transferred);
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeChannel> fake_channel_;
  mojo::Remote<mojom::MessageReceiver> message_receiver_remote_;
  mojo::Remote<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_remote_;
  std::unique_ptr<FakeClientChannelObserver> fake_observer_;

  mojom::ConnectionMetadataPtr connection_metadata_;
  base::OnceClosure message_sent_callback_;
  std::set<int> message_counters_received_;

  std::unique_ptr<ClientChannel> client_channel_;

 private:
  void OnGetConnectionMetadata(
      base::OnceClosure callback,
      mojom::ConnectionMetadataPtr connection_metadata) {
    connection_metadata_ = std::move(connection_metadata);
    std::move(callback).Run();
  }

  void OnMessageSent(int message_counter) {
    message_counters_received_.insert(message_counter);
    std::move(message_sent_callback_).Run();
  }
};

TEST_F(SecureChannelClientChannelImplTest, TestGetConnectionMetadata) {
  std::vector<mojom::ConnectionCreationDetail> creation_details{
      mojom::ConnectionCreationDetail::
          REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING};

  mojom::ConnectionMetadataPtr metadata = mojom::ConnectionMetadata::New(
      creation_details,
      mojom::BluetoothConnectionMetadata::New(-24 /* current_rssi */),
      "channel_binding_data");
  fake_channel_->set_connection_metadata_for_next_call(std::move(metadata));

  metadata = CallGetConnectionMetadata();
  EXPECT_EQ(creation_details, metadata->creation_details);
  EXPECT_EQ(-24, metadata->bluetooth_connection_metadata->current_rssi);
}

TEST_F(SecureChannelClientChannelImplTest, TestSendMessage) {
  int message_1_counter = CallSendMessage("payload1");
  int message_2_counter = CallSendMessage("payload2");

  std::vector<std::pair<std::string, mojom::Channel::SendMessageCallback>>&
      sent_messages = fake_channel_->sent_messages();

  EXPECT_EQ(2u, sent_messages.size());
  EXPECT_EQ("payload1", sent_messages[0].first);
  EXPECT_EQ("payload2", sent_messages[1].first);

  CallSendMessageCallback(std::move(sent_messages[0].second));
  CallSendMessageCallback(std::move(sent_messages[1].second));

  EXPECT_TRUE(base::Contains(message_counters_received_, message_1_counter));
  EXPECT_TRUE(base::Contains(message_counters_received_, message_2_counter));
}

TEST_F(SecureChannelClientChannelImplTest, TestReceiveMessage) {
  message_receiver_remote_->OnMessageReceived("payload");
  message_receiver_remote_.FlushForTesting();

  EXPECT_EQ(1u, fake_observer_->received_messages().size());
  EXPECT_EQ("payload", fake_observer_->received_messages()[0]);
}

TEST_F(SecureChannelClientChannelImplTest, TestDisconnectRemotely) {
  fake_channel_->DisconnectGeneratedRemote();

  SendPendingMojoMessages();

  VerifyChannelDisconnected();
}

TEST_F(SecureChannelClientChannelImplTest, TestNearbyConnectionStateChanged) {
  nearby_connection_state_listener_remote_->OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep::kRequestingConnectionStarted,
      mojom::NearbyConnectionStepResult::kSuccess);
  nearby_connection_state_listener_remote_.FlushForTesting();

  EXPECT_EQ(mojom::NearbyConnectionStep::kRequestingConnectionStarted,
            fake_observer_->nearby_connection_step());
  EXPECT_EQ(mojom::NearbyConnectionStepResult::kSuccess,
            fake_observer_->nearby_connection_step_result());
}

TEST_F(SecureChannelClientChannelImplTest, ReceiveMultipleFileTransferUpdates) {
  std::vector<mojom::FileTransferUpdatePtr> updates;

  client_channel_->RegisterPayloadFile(
      /*payload_id=*/1234, CreatePayloadFiles(),
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        updates.push_back(std::move(update));
      }),
      base::BindLambdaForTesting([&](bool success) { EXPECT_TRUE(success); }));
  SendPendingMojoMessages();
  EXPECT_EQ(1ul, fake_channel_->file_payload_listeners().size());
  EXPECT_TRUE(fake_channel_->file_payload_listeners().contains(1234));

  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/1000, /*bytes_transferred=*/100);
  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  EXPECT_EQ(2ul, updates.size());
  ExpectFileTransferUpdate(updates.at(0).get(),
                           mojom::FileTransferStatus::kInProgress,
                           /*total_bytes=*/1000, /*bytes_transferred=*/100);
  ExpectFileTransferUpdate(updates.at(1).get(),
                           mojom::FileTransferStatus::kSuccess,
                           /*total_bytes=*/1000, /*bytes_transferred=*/1000);
}

TEST_F(SecureChannelClientChannelImplTest, RegisterMultiplePayloadFiles) {
  std::vector<mojom::FileTransferUpdatePtr> first_payload_updates;
  std::vector<mojom::FileTransferUpdatePtr> second_payload_updates;

  client_channel_->RegisterPayloadFile(
      /*payload_id=*/1234, CreatePayloadFiles(),
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        first_payload_updates.push_back(std::move(update));
      }),
      base::BindLambdaForTesting([&](bool success) { EXPECT_TRUE(success); }));
  client_channel_->RegisterPayloadFile(
      /*payload_id=*/-5678, CreatePayloadFiles(),
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        second_payload_updates.push_back(std::move(update));
      }),
      base::BindLambdaForTesting([&](bool success) { EXPECT_TRUE(success); }));
  SendPendingMojoMessages();
  EXPECT_EQ(2ul, fake_channel_->file_payload_listeners().size());
  EXPECT_TRUE(fake_channel_->file_payload_listeners().contains(1234));
  EXPECT_TRUE(fake_channel_->file_payload_listeners().contains(-5678));

  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/-5678, mojom::FileTransferStatus::kFailure,
      /*total_bytes=*/2000, /*bytes_transferred=*/0);
  EXPECT_EQ(1ul, first_payload_updates.size());
  ExpectFileTransferUpdate(first_payload_updates.at(0).get(),
                           mojom::FileTransferStatus::kSuccess,
                           /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  EXPECT_EQ(1ul, second_payload_updates.size());
  ExpectFileTransferUpdate(second_payload_updates.at(0).get(),
                           mojom::FileTransferStatus::kFailure,
                           /*total_bytes=*/2000, /*bytes_transferred=*/0);
}

TEST_F(SecureChannelClientChannelImplTest,
       RemoteDisconnectsBeforeTransferComplete) {
  std::vector<mojom::FileTransferUpdatePtr> first_payload_updates;
  std::vector<mojom::FileTransferUpdatePtr> second_payload_updates;

  client_channel_->RegisterPayloadFile(
      /*payload_id=*/1234, CreatePayloadFiles(),
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        first_payload_updates.push_back(std::move(update));
      }),
      base::BindLambdaForTesting([&](bool success) { EXPECT_TRUE(success); }));
  client_channel_->RegisterPayloadFile(
      /*payload_id=*/-5678, CreatePayloadFiles(),
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        second_payload_updates.push_back(std::move(update));
      }),
      base::BindLambdaForTesting([&](bool success) { EXPECT_TRUE(success); }));
  SendPendingMojoMessages();

  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/-5678, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/2000, /*bytes_transferred=*/1000);

  fake_channel_->file_payload_listeners().at(1234).reset();
  fake_channel_->file_payload_listeners().at(-5678).reset();
  // Flush so the FilePayloadListener Receivers can get the disconnect message.
  SendPendingMojoMessages();

  EXPECT_EQ(1ul, first_payload_updates.size());
  // Incomplete transfers should get an additional cancelation update upon
  // disconnection.
  EXPECT_EQ(2ul, second_payload_updates.size());
  ExpectFileTransferUpdate(second_payload_updates.at(1).get(),
                           mojom::FileTransferStatus::kCanceled,
                           /*total_bytes=*/0, /*bytes_transferred=*/0);
}

TEST_F(SecureChannelClientChannelImplTest,
       ConnectionDestroyedBeforeTransferComplete) {
  std::vector<mojom::FileTransferUpdatePtr> first_payload_updates;
  std::vector<mojom::FileTransferUpdatePtr> second_payload_updates;

  client_channel_->RegisterPayloadFile(
      /*payload_id=*/1234, CreatePayloadFiles(),
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        first_payload_updates.push_back(std::move(update));
      }),
      base::BindLambdaForTesting([&](bool success) { EXPECT_TRUE(success); }));
  client_channel_->RegisterPayloadFile(
      /*payload_id=*/-5678, CreatePayloadFiles(),
      base::BindLambdaForTesting([&](mojom::FileTransferUpdatePtr update) {
        second_payload_updates.push_back(std::move(update));
      }),
      base::BindLambdaForTesting([&](bool success) { EXPECT_TRUE(success); }));
  SendPendingMojoMessages();

  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  fake_channel_->SendFileTransferUpdate(
      /*payload_id=*/-5678, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/2000, /*bytes_transferred=*/1000);

  client_channel_.reset();

  EXPECT_EQ(1ul, first_payload_updates.size());
  // Incomplete transfers should get an additional cancelation update upon
  // disconnection.
  EXPECT_EQ(2ul, second_payload_updates.size());
  ExpectFileTransferUpdate(second_payload_updates.at(1).get(),
                           mojom::FileTransferStatus::kCanceled,
                           /*total_bytes=*/0, /*bytes_transferred=*/0);
}

}  // namespace ash::secure_channel
