// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/multiplexed_channel_impl.h"

#include <iterator>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/connection_medium.h"
#include "chromeos/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/services/secure_channel/fake_multiplexed_channel.h"
#include "chromeos/services/secure_channel/fake_single_client_message_proxy.h"
#include "chromeos/services/secure_channel/single_client_message_proxy_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const char kTestDeviceId[] = "testDeviceId";
const char kTestFeature[] = "testFeature";

class FakeSingleClientMessageProxyImplFactory
    : public SingleClientMessageProxyImpl::Factory {
 public:
  FakeSingleClientMessageProxyImplFactory() = default;
  ~FakeSingleClientMessageProxyImplFactory() override = default;

  const SingleClientMessageProxy::Delegate* expected_delegate() {
    return expected_delegate_;
  }

  // Contains all created FakeSingleClientMessageProxy pointers which have not
  // yet been deleted.
  std::unordered_map<base::UnguessableToken,
                     FakeSingleClientMessageProxy*,
                     base::UnguessableTokenHash>&
  id_to_active_proxy_map() {
    return id_to_active_proxy_map_;
  }

 private:
  std::unique_ptr<SingleClientMessageProxy> BuildInstance(
      SingleClientMessageProxy::Delegate* delegate,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters)
      override {
    EXPECT_EQ(kTestFeature, client_connection_parameters->feature());
    EXPECT_TRUE(client_connection_parameters->IsClientWaitingForResponse());

    if (!expected_delegate_)
      expected_delegate_ = delegate;
    // Each call should have the same delegate.
    EXPECT_EQ(expected_delegate_, delegate);

    std::unique_ptr<SingleClientMessageProxy> proxy = std::make_unique<
        FakeSingleClientMessageProxy>(
        delegate,
        base::BindOnce(
            &FakeSingleClientMessageProxyImplFactory::OnCreatedInstanceDeleted,
            base::Unretained(this)));
    FakeSingleClientMessageProxy* proxy_raw =
        static_cast<FakeSingleClientMessageProxy*>(proxy.get());
    id_to_active_proxy_map_[proxy->GetProxyId()] = proxy_raw;

    return proxy;
  }

  void OnCreatedInstanceDeleted(
      const base::UnguessableToken& deleted_proxy_id) {
    size_t num_deleted = id_to_active_proxy_map_.erase(deleted_proxy_id);
    EXPECT_EQ(1u, num_deleted);
  }

  SingleClientMessageProxy::Delegate* expected_delegate_ = nullptr;
  std::unordered_map<base::UnguessableToken,
                     FakeSingleClientMessageProxy*,
                     base::UnguessableTokenHash>
      id_to_active_proxy_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeSingleClientMessageProxyImplFactory);
};

}  // namespace

class SecureChannelMultiplexedChannelImplTest : public testing::Test {
 protected:
  SecureChannelMultiplexedChannelImplTest() = default;
  ~SecureChannelMultiplexedChannelImplTest() override = default;

  void SetUp() override {
    fake_proxy_factory_ =
        std::make_unique<FakeSingleClientMessageProxyImplFactory>();
    SingleClientMessageProxyImpl::Factory::SetInstanceForTesting(
        fake_proxy_factory_.get());

    fake_delegate_ = std::make_unique<FakeMultiplexedChannelDelegate>();

    // The default list contains one client.
    initial_client_list_.push_back(
        std::make_unique<FakeClientConnectionParameters>(kTestFeature));
  }

  void TearDown() override {
    SingleClientMessageProxyImpl::Factory::SetInstanceForTesting(nullptr);
  }

  void CreateChannel() {
    auto fake_authenticated_channel =
        std::make_unique<FakeAuthenticatedChannel>();
    fake_authenticated_channel_ = fake_authenticated_channel.get();

    multiplexed_channel_ =
        MultiplexedChannelImpl::Factory::Get()->BuildInstance(
            std::move(fake_authenticated_channel), fake_delegate_.get(),
            ConnectionDetails(kTestDeviceId,
                              ConnectionMedium::kBluetoothLowEnergy),
            &initial_client_list_);

    // Once BuildInstance() has finished, |fake_proxy_factory_| is expected to
    // have already created one or more instances. Verify that the delegate
    // passed to the factory is actually |multiplexed_channel_|.
    EXPECT_EQ(static_cast<MultiplexedChannelImpl*>(multiplexed_channel_.get()),
              fake_proxy_factory_->expected_delegate());
  }

  // If |complete_sending| is true, the "on sent" callback is invoked.
  int SendMessageAndVerifyState(FakeSingleClientMessageProxy* sending_proxy,
                                const std::string& feature,
                                const std::string& payload,
                                bool complete_sending = true) {
    size_t num_sent_message_before_call = sent_messages().size();

    int message_counter = next_send_message_counter_++;
    sending_proxy->NotifySendMessageRequested(
        feature, payload,
        base::BindOnce(&SecureChannelMultiplexedChannelImplTest::OnMessageSent,
                       base::Unretained(this), message_counter));

    EXPECT_EQ(num_sent_message_before_call + 1u, sent_messages().size());
    EXPECT_EQ(feature, std::get<0>(sent_messages().back()));
    EXPECT_EQ(payload, std::get<1>(sent_messages().back()));

    if (complete_sending) {
      EXPECT_FALSE(HasMessageBeenSent(message_counter));
      std::move(std::get<2>(sent_messages().back())).Run();
      EXPECT_TRUE(HasMessageBeenSent(message_counter));
    }

    return message_counter;
  }

  void ReceiveMessageAndVerifyState(const std::string& feature,
                                    const std::string& payload) {
    std::unordered_map<base::UnguessableToken, size_t,
                       base::UnguessableTokenHash>
        proxy_id_to_num_processed_messages_before_call_map;
    for (auto& map_entry : id_to_active_proxy_map()) {
      proxy_id_to_num_processed_messages_before_call_map[map_entry.first] =
          map_entry.second->processed_messages().size();
    }

    fake_authenticated_channel_->NotifyMessageReceived(feature, payload);

    for (auto& map_entry : id_to_active_proxy_map()) {
      EXPECT_EQ(
          proxy_id_to_num_processed_messages_before_call_map[map_entry.first] +
              1u,
          map_entry.second->processed_messages().size());
      EXPECT_EQ(feature, map_entry.second->processed_messages().back().first);
      EXPECT_EQ(payload, map_entry.second->processed_messages().back().second);
    }
  }

  bool HasMessageBeenSent(int message_counter) {
    return base::Contains(sent_message_counters_, message_counter);
  }

  void DisconnectClientAndVerifyState(
      FakeSingleClientMessageProxy* sending_proxy,
      bool expected_to_be_last_client) {
    // If this is the last client left, disconnecting it should result in the
    // underlying channel becoming disconnected.
    bool is_last_client = id_to_active_proxy_map().size() == 1u;
    EXPECT_EQ(is_last_client, expected_to_be_last_client);

    base::UnguessableToken proxy_id = sending_proxy->GetProxyId();

    // All relevant parties should still indicate that the connection is valid.
    EXPECT_TRUE(base::Contains(id_to_active_proxy_map(), proxy_id));
    EXPECT_FALSE(
        fake_authenticated_channel_->has_disconnection_been_requested());
    EXPECT_FALSE(multiplexed_channel_->IsDisconnecting());
    EXPECT_FALSE(multiplexed_channel_->IsDisconnected());
    EXPECT_FALSE(fake_delegate_->disconnected_connection_details());

    // Disconnecting the client should result in the proxy being deleted.
    sending_proxy->NotifyClientDisconnected();
    EXPECT_FALSE(base::Contains(id_to_active_proxy_map(), proxy_id));

    if (!is_last_client)
      return;

    // Since this was the last client, |multiplexed_channel_| should have
    // started the disconnection flow. Because disconnection is asynchronous,
    // it should have not yet completed yet.
    EXPECT_TRUE(multiplexed_channel_->IsDisconnecting());
    EXPECT_FALSE(multiplexed_channel_->IsDisconnected());
    EXPECT_TRUE(
        fake_authenticated_channel_->has_disconnection_been_requested());
    EXPECT_FALSE(fake_delegate_->disconnected_connection_details());

    // Complete asynchronous disconnection.
    fake_authenticated_channel_->NotifyDisconnected();

    // Verify that all relevant parties have been notified of the disconnection.
    EXPECT_EQ(multiplexed_channel_->connection_details(),
              fake_delegate_->disconnected_connection_details());
    EXPECT_FALSE(multiplexed_channel_->IsDisconnecting());
    EXPECT_TRUE(multiplexed_channel_->IsDisconnected());
  }

  void DisconnectRemoteDeviceAndVerifyState() {
    // Before the disconnection, all relevant parties should still indicate that
    // the connection is valid.
    EXPECT_FALSE(multiplexed_channel_->IsDisconnecting());
    EXPECT_FALSE(multiplexed_channel_->IsDisconnected());
    EXPECT_FALSE(fake_delegate_->disconnected_connection_details());
    for (auto& map_entry : id_to_active_proxy_map())
      EXPECT_FALSE(map_entry.second->was_remote_device_disconnection_handled());

    fake_authenticated_channel_->NotifyDisconnected();

    // Verify that the above preconditions have now been changed.
    EXPECT_FALSE(multiplexed_channel_->IsDisconnecting());
    EXPECT_TRUE(multiplexed_channel_->IsDisconnected());
    EXPECT_EQ(multiplexed_channel_->connection_details(),
              fake_delegate_->disconnected_connection_details());
    for (auto& map_entry : id_to_active_proxy_map())
      EXPECT_TRUE(map_entry.second->was_remote_device_disconnection_handled());
  }

  void AddClientToChannel(const std::string& feature) {
    bool success = multiplexed_channel_->AddClientToChannel(
        std::make_unique<FakeClientConnectionParameters>(feature));
    EXPECT_TRUE(success);
  }

  void CallGetConnectionMetadataFromDelegate(
      FakeSingleClientMessageProxy* proxy) {
    proxy->GetConnectionMetadataFromDelegate(base::BindOnce(
        &SecureChannelMultiplexedChannelImplTest::OnGetConnectionMetadata,
        base::Unretained(this)));
  }

  void OnGetConnectionMetadata(
      mojom::ConnectionMetadataPtr connection_metadata) {
    connection_metadata_ = std::move(connection_metadata);
  }

  std::vector<std::unique_ptr<ClientConnectionParameters>>&
  initial_client_list() {
    return initial_client_list_;
  }

  std::unordered_map<base::UnguessableToken,
                     FakeSingleClientMessageProxy*,
                     base::UnguessableTokenHash>&
  id_to_active_proxy_map() {
    return fake_proxy_factory_->id_to_active_proxy_map();
  }

  std::vector<std::tuple<std::string, std::string, base::OnceClosure>>&
  sent_messages() {
    return fake_authenticated_channel_->sent_messages();
  }

  FakeAuthenticatedChannel* fake_authenticated_channel() {
    return fake_authenticated_channel_;
  }

  mojom::ConnectionMetadataPtr connection_metadata_;

 private:
  void OnMessageSent(int message_counter) {
    sent_message_counters_.insert(message_counter);
  }

  base::test::TaskEnvironment task_environment_;

  int next_send_message_counter_ = 0;
  std::unordered_set<int> sent_message_counters_;

  std::unique_ptr<FakeSingleClientMessageProxyImplFactory> fake_proxy_factory_;

  std::vector<std::unique_ptr<ClientConnectionParameters>> initial_client_list_;

  FakeAuthenticatedChannel* fake_authenticated_channel_ = nullptr;
  std::unique_ptr<FakeMultiplexedChannelDelegate> fake_delegate_;

  std::unique_ptr<MultiplexedChannel> multiplexed_channel_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelMultiplexedChannelImplTest);
};

TEST_F(SecureChannelMultiplexedChannelImplTest, ConnectionMetadata) {
  CreateChannel();
  EXPECT_EQ(1u, id_to_active_proxy_map().size());

  std::vector<mojom::ConnectionCreationDetail> creation_details{
      mojom::ConnectionCreationDetail::
          REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING};

  // Set connection metadata on |fake_authenticated_channel_|.
  mojom::ConnectionMetadataPtr metadata = mojom::ConnectionMetadata::New(
      creation_details,
      mojom::BluetoothConnectionMetadata::New(-24 /* current_rssi */),
      "channel_binding_data_1");
  fake_authenticated_channel()->set_connection_metadata_for_next_call(
      std::move(metadata));

  // Retrieving the metadata through the proxy should cause
  // |fake_authenticated_channel_|'s metadata to be passed through
  // |multiplexed_channel_|.
  CallGetConnectionMetadataFromDelegate(
      id_to_active_proxy_map().begin()->second);
  EXPECT_EQ(creation_details, connection_metadata_->creation_details);
  EXPECT_EQ(-24,
            connection_metadata_->bluetooth_connection_metadata->current_rssi);
  EXPECT_EQ("channel_binding_data_1",
            connection_metadata_->channel_binding_data);

  // Now, change the values and set them on |fake_authenticated_channel_|.
  creation_details.clear();
  metadata = mojom::ConnectionMetadata::New(
      creation_details,
      mojom::BluetoothConnectionMetadata::New(-8 /* current_rssi */),
      "channel_binding_data_2");
  fake_authenticated_channel()->set_connection_metadata_for_next_call(
      std::move(metadata));

  // The new updates should be available.
  CallGetConnectionMetadataFromDelegate(
      id_to_active_proxy_map().begin()->second);
  EXPECT_EQ(creation_details, connection_metadata_->creation_details);
  EXPECT_EQ(-8,
            connection_metadata_->bluetooth_connection_metadata->current_rssi);
  EXPECT_EQ("channel_binding_data_2",
            connection_metadata_->channel_binding_data);

  DisconnectClientAndVerifyState(id_to_active_proxy_map().begin()->second,
                                 true /* expected_to_be_last_client */);
}

TEST_F(SecureChannelMultiplexedChannelImplTest,
       OneClient_SendReceive_DisconnectFromClient) {
  CreateChannel();
  EXPECT_EQ(1u, id_to_active_proxy_map().size());

  SendMessageAndVerifyState(
      id_to_active_proxy_map().begin()->second /* sending_proxy */, "feature1",
      "payload1");
  ReceiveMessageAndVerifyState("feature2", "payload2");

  DisconnectClientAndVerifyState(id_to_active_proxy_map().begin()->second,
                                 true /* expected_to_be_last_client */);
}

TEST_F(SecureChannelMultiplexedChannelImplTest,
       OneClient_SendAsync_DisconnectFromRemoteDevice) {
  CreateChannel();
  EXPECT_EQ(1u, id_to_active_proxy_map().size());

  // Start sending two messages, but do not complete the asynchronous sending.
  int counter1 = SendMessageAndVerifyState(
      id_to_active_proxy_map().begin()->second /* sending_proxy */, "feature1",
      "payload1", false /* complete_sending */);
  int counter2 = SendMessageAndVerifyState(
      id_to_active_proxy_map().begin()->second /* sending_proxy */, "feature2",
      "payload2", false /* complete_sending */);
  EXPECT_FALSE(HasMessageBeenSent(counter1));
  EXPECT_FALSE(HasMessageBeenSent(counter2));
  EXPECT_EQ(2u, sent_messages().size());

  // Finish sending the first message.
  std::move(std::get<2>(sent_messages()[0])).Run();
  EXPECT_TRUE(HasMessageBeenSent(counter1));
  EXPECT_FALSE(HasMessageBeenSent(counter2));

  // Before the second message is sent successfully, disconnect.
  DisconnectRemoteDeviceAndVerifyState();
}

TEST_F(SecureChannelMultiplexedChannelImplTest,
       TwoInitialClient_OneAdded_DisconnectFromClient) {
  // Add a second initial client.
  initial_client_list().push_back(
      std::make_unique<FakeClientConnectionParameters>(kTestFeature));

  CreateChannel();
  EXPECT_EQ(2u, id_to_active_proxy_map().size());

  // Send a message from the first client.
  SendMessageAndVerifyState(
      id_to_active_proxy_map().begin()->second /* sending_proxy */, "feature1",
      "payload1");

  // Receive a message (both clients should receive).
  ReceiveMessageAndVerifyState("feature2", "payload2");

  // Send a message from the second client.
  auto it = id_to_active_proxy_map().begin();
  std::advance(it, 1);
  SendMessageAndVerifyState(it->second /* sending_proxy */, "feature3",
                            "payload3");

  // Receive a message (both clients should receive).
  ReceiveMessageAndVerifyState("feature4", "payload4");

  // Create a third client and add it to the channel.
  AddClientToChannel(kTestFeature);
  EXPECT_EQ(3u, id_to_active_proxy_map().size());

  // Send a message from the third client.
  it = id_to_active_proxy_map().begin();
  std::advance(it, 2);
  SendMessageAndVerifyState(it->second, "feature5", "payload5");
  // Receive a message (both clients should receive).
  ReceiveMessageAndVerifyState("feature6", "payload6");

  // Disconnect the clients, one by one. Only the last client should actually
  // trigger |fake_authenticated_channel_| to disconnect.
  DisconnectClientAndVerifyState(id_to_active_proxy_map().begin()->second,
                                 false /* expected_to_be_last_client */);
  EXPECT_EQ(2u, id_to_active_proxy_map().size());
  DisconnectClientAndVerifyState(id_to_active_proxy_map().begin()->second,
                                 false /* expected_to_be_last_client */);
  EXPECT_EQ(1u, id_to_active_proxy_map().size());
  DisconnectClientAndVerifyState(id_to_active_proxy_map().begin()->second,
                                 true /* expected_to_be_last_client */);
}

}  // namespace secure_channel

}  // namespace chromeos
