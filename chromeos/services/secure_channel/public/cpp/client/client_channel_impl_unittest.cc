// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/client_channel_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/secure_channel/fake_channel.h"
#include "chromeos/services/secure_channel/fake_secure_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel_observer.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/secure_channel_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelClientChannelImplTest : public testing::Test {
 protected:
  SecureChannelClientChannelImplTest() = default;

  // testing::Test:
  void SetUp() override {
    fake_channel_ = std::make_unique<FakeChannel>();

    client_channel_ = ClientChannelImpl::Factory::Get()->BuildInstance(
        fake_channel_->GenerateRemote(),
        message_receiver_remote_.BindNewPipeAndPassReceiver());

    fake_observer_ = std::make_unique<FakeClientChannelObserver>();
    client_channel_->AddObserver(fake_observer_.get());
  }

  void TearDown() override {
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

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeChannel> fake_channel_;
  mojo::Remote<mojom::MessageReceiver> message_receiver_remote_;
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

  DISALLOW_COPY_AND_ASSIGN(SecureChannelClientChannelImplTest);
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

}  // namespace secure_channel

}  // namespace chromeos
