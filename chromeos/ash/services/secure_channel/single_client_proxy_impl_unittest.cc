// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/single_client_proxy_impl.h"

#include <memory>
#include <string>
#include <unordered_set>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/fake_file_payload_listener.h"
#include "chromeos/ash/services/secure_channel/fake_message_receiver.h"
#include "chromeos/ash/services/secure_channel/fake_nearby_connection_state_listener.h"
#include "chromeos/ash/services/secure_channel/fake_single_client_proxy.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const char kTestFeature[] = "testFeature";

}  // namespace

class SecureChannelSingleClientProxyImplTest : public testing::Test {
 protected:
  SecureChannelSingleClientProxyImplTest() = default;
  SecureChannelSingleClientProxyImplTest(
      const SecureChannelSingleClientProxyImplTest&) = delete;
  SecureChannelSingleClientProxyImplTest& operator=(
      const SecureChannelSingleClientProxyImplTest&) = delete;
  ~SecureChannelSingleClientProxyImplTest() override {
    fake_client_connection_parameters_ = nullptr;
    fake_message_receiver_ = nullptr;
    fake_nearby_connection_state_listener_ = nullptr;
  }

  void SetUp() override {
    fake_proxy_delegate_ = std::make_unique<FakeSingleClientProxyDelegate>();

    auto fake_message_receiver = std::make_unique<FakeMessageReceiver>();
    fake_message_receiver_ = fake_message_receiver.get();

    auto fake_nearby_connection_state_listener =
        std::make_unique<FakeNearbyConnectionStateListener>();
    fake_nearby_connection_state_listener_ =
        fake_nearby_connection_state_listener.get();

    auto fake_client_connection_parameters =
        std::make_unique<FakeClientConnectionParameters>(kTestFeature);
    fake_client_connection_parameters_ =
        fake_client_connection_parameters.get();
    fake_client_connection_parameters_->set_message_receiver(
        std::move(fake_message_receiver));
    fake_client_connection_parameters_->set_nearby_connection_state_listener(
        std::move(fake_nearby_connection_state_listener));

    proxy_ = SingleClientProxyImpl::Factory::Create(
        fake_proxy_delegate_.get(),
        std::move(fake_client_connection_parameters));

    CompletePendingMojoCalls();
    EXPECT_TRUE(fake_client_connection_parameters_->channel());
  }

  void CompletePendingMojoCalls() {
    // FlushForTesting is a function on SingleClientProxyImpl, so a cast
    // is necessary.
    auto* proxy = static_cast<SingleClientProxyImpl*>(proxy_.get());
    proxy->FlushForTesting();
  }

  void TearDown() override {}

  // If |complete_sending| is true, the "on sent" callback is invoked.
  int SendMessageAndVerifyState(const std::string& message,
                                bool complete_sending = true) {
    auto& send_message_requests =
        fake_proxy_delegate()->send_message_requests();
    size_t num_send_message_requests_before_call = send_message_requests.size();

    int message_counter = next_message_counter_++;

    mojo::Remote<mojom::Channel>& channel =
        fake_client_connection_parameters_->channel();
    channel->SendMessage(
        message,
        base::BindOnce(&SecureChannelSingleClientProxyImplTest::OnMessageSent,
                       base::Unretained(this), message_counter));
    channel.FlushForTesting();

    EXPECT_EQ(num_send_message_requests_before_call + 1u,
              send_message_requests.size());
    EXPECT_EQ(kTestFeature, std::get<0>(send_message_requests.back()));
    EXPECT_EQ(message, std::get<1>(send_message_requests.back()));
    EXPECT_FALSE(WasMessageSent(message_counter));

    if (complete_sending) {
      std::move(std::get<2>(send_message_requests.back())).Run();
      CompletePendingMojoCalls();
      EXPECT_TRUE(WasMessageSent(message_counter));
    }

    return message_counter;
  }

  void HandleReceivedMessageAndVerifyState(const std::string& feature,
                                           const std::string& payload) {
    const std::vector<std::string>& received_messages =
        fake_message_receiver_->received_messages();
    size_t num_received_messages_before_call = received_messages.size();

    proxy_->HandleReceivedMessage(feature, payload);
    CompletePendingMojoCalls();

    // If message's feature was not the type specified by the client, no
    // additional message should have been passed to |fake_message_receiver_|.
    if (feature != kTestFeature) {
      EXPECT_EQ(num_received_messages_before_call, received_messages.size());
      return;
    }

    // Otherwise, a message should have been passed.
    EXPECT_EQ(num_received_messages_before_call + 1u, received_messages.size());
    EXPECT_EQ(payload, received_messages.back());
  }

  void HandleNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result) {
    proxy_->HandleNearbyConnectionStateChanged(nearby_connection_step, result);
    CompletePendingMojoCalls();

    EXPECT_EQ(nearby_connection_step,
              fake_nearby_connection_state_listener_->nearby_connection_step());
    EXPECT_EQ(result, fake_nearby_connection_state_listener_
                          ->nearby_connection_step_result());
  }

  FakeSingleClientProxyDelegate* fake_proxy_delegate() {
    return fake_proxy_delegate_.get();
  }

  FakeMessageReceiver* fake_message_receiver() {
    return fake_message_receiver_;
  }

  bool WasMessageSent(int message_counter) {
    return base::Contains(sent_message_counters_, message_counter);
  }

  void DisconnectFromClientSide() {
    EXPECT_FALSE(WasDelegateNotifiedOfDisconnection());

    base::RunLoop run_loop;
    fake_proxy_delegate_->set_on_client_disconnected_closure(
        run_loop.QuitClosure());
    fake_client_connection_parameters_->channel().reset();
    run_loop.Run();

    EXPECT_TRUE(WasDelegateNotifiedOfDisconnection());
  }

  void DisconnectFromRemoteDeviceSide() {
    EXPECT_TRUE(fake_client_connection_parameters_->channel());

    proxy_->HandleRemoteDeviceDisconnection();
    CompletePendingMojoCalls();

    EXPECT_FALSE(fake_client_connection_parameters_->channel());
    EXPECT_EQ(static_cast<uint32_t>(mojom::Channel::kConnectionDroppedReason),
              fake_client_connection_parameters_->disconnection_reason());
  }

  bool WasDelegateNotifiedOfDisconnection() {
    return proxy_->GetProxyId() ==
           fake_proxy_delegate_->disconnected_proxy_id();
  }

  mojom::ConnectionMetadataPtr GetConnectionMetadataFromChannel() {
    EXPECT_FALSE(last_metadata_from_channel_);

    mojo::Remote<mojom::Channel>& channel =
        fake_client_connection_parameters_->channel();
    channel->GetConnectionMetadata(base::BindOnce(
        &SecureChannelSingleClientProxyImplTest::OnConnectionMetadata,
        base::Unretained(this)));
    channel.FlushForTesting();

    return std::move(last_metadata_from_channel_);
  }

  void RegisterPayloadFileAndVerifyResult(
      int64_t payload_id,
      bool expect_success,
      FakeFilePayloadListener& fake_file_payload_listener) {
    base::FilePath file_path;
    base::CreateTemporaryFile(&file_path);
    base::File input_file(
        file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    base::File output_file(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                          base::File::Flags::FLAG_WRITE);

    mojo::PendingRemote<mojom::FilePayloadListener>
        file_payload_listener_remote =
            fake_file_payload_listener.GenerateRemote();

    size_t old_registration_count =
        fake_proxy_delegate()->register_payload_file_requests().size();

    fake_proxy_delegate()->set_register_payload_file_result(expect_success);

    mojo::Remote<mojom::Channel>& channel =
        fake_client_connection_parameters_->channel();
    channel->RegisterPayloadFile(
        payload_id,
        mojom::PayloadFiles::New(std::move(input_file), std::move(output_file)),
        std::move(file_payload_listener_remote),
        base::BindLambdaForTesting(
            [&](bool success) { EXPECT_EQ(success, expect_success); }));
    channel.FlushForTesting();

    EXPECT_EQ(++old_registration_count,
              fake_proxy_delegate()->register_payload_file_requests().size());
    EXPECT_TRUE(
        fake_proxy_delegate()->register_payload_file_requests().contains(
            payload_id));
  }

  void SendFileTransferUpdateAndVerifyResult(
      int64_t payload_id,
      mojom::FileTransferStatus status,
      uint64_t total_bytes,
      uint64_t bytes_transferred,
      size_t expected_update_count,
      FakeFilePayloadListener& fake_file_payload_listener) {
    mojom::FileTransferUpdate expected_update = mojom::FileTransferUpdate(
        payload_id, status, total_bytes, bytes_transferred);

    fake_proxy_delegate()
        ->register_payload_file_requests()
        .at(payload_id)
        .file_transfer_update_callback.Run(expected_update.Clone());
    fake_file_payload_listener.receiver().FlushForTesting();

    EXPECT_EQ(expected_update_count,
              fake_file_payload_listener.received_updates().size());
    EXPECT_EQ(expected_update,
              *fake_file_payload_listener.received_updates().back());
  }

 private:
  void OnMessageSent(int message_counter) {
    sent_message_counters_.insert(message_counter);
  }

  void OnConnectionMetadata(
      mojom::ConnectionMetadataPtr connection_metadata_ptr) {
    last_metadata_from_channel_ = std::move(connection_metadata_ptr);
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeSingleClientProxyDelegate> fake_proxy_delegate_;
  raw_ptr<FakeClientConnectionParameters> fake_client_connection_parameters_;
  raw_ptr<FakeMessageReceiver> fake_message_receiver_;
  raw_ptr<FakeNearbyConnectionStateListener>
      fake_nearby_connection_state_listener_;

  int next_message_counter_ = 0;
  std::unordered_set<int> sent_message_counters_;

  mojom::ConnectionMetadataPtr last_metadata_from_channel_;

  std::unique_ptr<SingleClientProxy> proxy_;
};

TEST_F(SecureChannelSingleClientProxyImplTest,
       SendReceiveAndDisconnect_ClientDisconnection) {
  SendMessageAndVerifyState("message1");
  HandleReceivedMessageAndVerifyState(kTestFeature, "message2");
  DisconnectFromClientSide();
}

TEST_F(SecureChannelSingleClientProxyImplTest,
       SendReceiveAndDisconnect_RemoteDeviceDisconnection) {
  SendMessageAndVerifyState("message1");
  HandleReceivedMessageAndVerifyState(kTestFeature, "message2");

  DisconnectFromRemoteDeviceSide();
}

TEST_F(SecureChannelSingleClientProxyImplTest, SendWithDeferredCompletion) {
  auto& send_message_requests = fake_proxy_delegate()->send_message_requests();

  // Send two messages, but do not wait for the first to send successfully
  // before sending the second one.
  int counter1 =
      SendMessageAndVerifyState("message1", false /* complete_sending */);
  int counter2 =
      SendMessageAndVerifyState("message2", false /* complete_sending */);
  EXPECT_EQ(2u, send_message_requests.size());
  EXPECT_FALSE(WasMessageSent(counter1));
  EXPECT_FALSE(WasMessageSent(counter2));

  // Complete sending the first message.
  std::move(std::get<2>(send_message_requests[0])).Run();
  CompletePendingMojoCalls();
  EXPECT_TRUE(WasMessageSent(counter1));

  // Before the second one completes, disconnect from the remote side.
  DisconnectFromRemoteDeviceSide();
}

TEST_F(SecureChannelSingleClientProxyImplTest,
       ReceiveMessagesFromMultipleFeatures) {
  HandleReceivedMessageAndVerifyState(kTestFeature, "message1");
  HandleReceivedMessageAndVerifyState("otherFeature", "message2");
  DisconnectFromRemoteDeviceSide();
}

TEST_F(SecureChannelSingleClientProxyImplTest, NearbyConnectionStateChanged) {
  HandleNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep::kUpgradedToWebRtc,
      mojom::NearbyConnectionStepResult::kSuccess);
  DisconnectFromRemoteDeviceSide();
}

TEST_F(SecureChannelSingleClientProxyImplTest, ConnectionMetadata) {
  std::vector<mojom::ConnectionCreationDetail> creation_details{
      mojom::ConnectionCreationDetail::
          REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING};

  mojom::ConnectionMetadataPtr metadata = mojom::ConnectionMetadata::New(
      creation_details,
      mojom::BluetoothConnectionMetadata::New(-24 /* current_rssi */),
      "channel_binding_data");
  fake_proxy_delegate()->set_connection_metadata_for_next_call(
      std::move(metadata));

  metadata = GetConnectionMetadataFromChannel();
  EXPECT_EQ(creation_details, metadata->creation_details);
  EXPECT_EQ(-24, metadata->bluetooth_connection_metadata->current_rssi);
}

TEST_F(SecureChannelSingleClientProxyImplTest,
       RegisterOnePayloadFileAndReceiveMultipleUpdates) {
  FakeFilePayloadListener fake_file_payload_listener;

  RegisterPayloadFileAndVerifyResult(/*payload_id=*/1234,
                                     /*expect_success=*/true,
                                     fake_file_payload_listener);

  SendFileTransferUpdateAndVerifyResult(
      /*payload_id=*/1234, mojom::FileTransferStatus::kInProgress,
      /*total_bytes=*/1000, /*bytes_transferred=*/100,
      /*expected_update_count=*/1, fake_file_payload_listener);
  EXPECT_TRUE(fake_file_payload_listener.is_connected());

  SendFileTransferUpdateAndVerifyResult(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000,
      /*expected_update_count=*/2, fake_file_payload_listener);
  EXPECT_FALSE(fake_file_payload_listener.is_connected());
}

TEST_F(SecureChannelSingleClientProxyImplTest,
       RegisterMultiplePayloadFilesAndReceiveUpdates) {
  FakeFilePayloadListener first_payload_listener;
  FakeFilePayloadListener second_payload_listener;

  RegisterPayloadFileAndVerifyResult(/*payload_id=*/1234,
                                     /*expect_sucess=*/true,
                                     first_payload_listener);
  RegisterPayloadFileAndVerifyResult(/*payload_id=*/-5678,
                                     /*expect_sucess=*/true,
                                     second_payload_listener);

  SendFileTransferUpdateAndVerifyResult(
      /*payload_id=*/1234, mojom::FileTransferStatus::kSuccess,
      /*total_bytes=*/1000, /*bytes_transferred=*/1000,
      /*expected_update_count=*/1, first_payload_listener);
  EXPECT_FALSE(first_payload_listener.is_connected());

  SendFileTransferUpdateAndVerifyResult(
      /*payload_id=*/-5678, mojom::FileTransferStatus::kFailure,
      /*total_bytes=*/2000, /*bytes_transferred=*/0,
      /*expected_update_count=*/1, second_payload_listener);
  EXPECT_FALSE(second_payload_listener.is_connected());
}

TEST_F(SecureChannelSingleClientProxyImplTest,
       RemoteDeviceDisconnectsBeforeTransfersComplete) {
  FakeFilePayloadListener fake_file_payload_listener;
  RegisterPayloadFileAndVerifyResult(/*payload_id=*/1234,
                                     /*expect_sucess=*/true,
                                     fake_file_payload_listener);

  // Disconnect from remote device before transfer of the second payload is
  // complete.
  DisconnectFromRemoteDeviceSide();
  fake_file_payload_listener.receiver().FlushForTesting();
  EXPECT_FALSE(fake_file_payload_listener.is_connected());
}

TEST_F(SecureChannelSingleClientProxyImplTest, RegisterPayloadFileFails) {
  FakeFilePayloadListener fake_file_payload_listener;
  RegisterPayloadFileAndVerifyResult(/*payload_id=*/1234,
                                     /*expect_sucess=*/false,
                                     fake_file_payload_listener);

  fake_file_payload_listener.receiver().FlushForTesting();
  EXPECT_FALSE(fake_file_payload_listener.is_connected());
}

}  // namespace ash::secure_channel
