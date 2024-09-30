// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/active_connection_manager_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/connection_details.h"
#include "chromeos/ash/services/secure_channel/fake_active_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/fake_multiplexed_channel.h"
#include "chromeos/ash/services/secure_channel/multiplexed_channel_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

class FakeMultiplexedChannelFactory : public MultiplexedChannelImpl::Factory {
 public:
  explicit FakeMultiplexedChannelFactory(
      MultiplexedChannel::Delegate* expected_delegate)
      : expected_delegate_(expected_delegate) {}

  FakeMultiplexedChannelFactory(const FakeMultiplexedChannelFactory&) = delete;
  FakeMultiplexedChannelFactory& operator=(
      const FakeMultiplexedChannelFactory&) = delete;

  ~FakeMultiplexedChannelFactory() override = default;

  base::flat_map<ConnectionDetails,
                 raw_ptr<FakeMultiplexedChannel, CtnExperimental>>&
  connection_details_to_active_channel_map() {
    return connection_details_to_active_channel_map_;
  }

  void set_next_expected_authenticated_channel(
      AuthenticatedChannel* authenticated_channel) {
    next_expected_authenticated_channel_ = authenticated_channel;
  }

  // MultiplexedChannelImpl::Factory:
  std::unique_ptr<MultiplexedChannel> CreateInstance(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      MultiplexedChannel::Delegate* delegate,
      ConnectionDetails connection_details,
      std::vector<std::unique_ptr<ClientConnectionParameters>>* initial_clients)
      override {
    EXPECT_EQ(expected_delegate_, delegate);
    EXPECT_EQ(next_expected_authenticated_channel_,
              authenticated_channel.get());
    next_expected_authenticated_channel_ = nullptr;

    auto fake_channel = std::make_unique<FakeMultiplexedChannel>(
        delegate, connection_details,
        base::BindOnce(&FakeMultiplexedChannelFactory::OnChannelDeleted,
                       base::Unretained(this)));

    for (auto& initial_client : *initial_clients)
      fake_channel->AddClientToChannel(std::move(initial_client));

    connection_details_to_active_channel_map_[connection_details] =
        fake_channel.get();

    return fake_channel;
  }

 private:
  void OnChannelDeleted(const ConnectionDetails& connection_details) {
    size_t num_deleted =
        connection_details_to_active_channel_map_.erase(connection_details);
    EXPECT_EQ(1u, num_deleted);
  }

  raw_ptr<const MultiplexedChannel::Delegate, DanglingUntriaged>
      expected_delegate_;

  raw_ptr<AuthenticatedChannel> next_expected_authenticated_channel_ = nullptr;

  base::flat_map<ConnectionDetails,
                 raw_ptr<FakeMultiplexedChannel, CtnExperimental>>
      connection_details_to_active_channel_map_;
};

std::vector<base::UnguessableToken> ClientListToIdList(
    const std::vector<std::unique_ptr<ClientConnectionParameters>>&
        client_list) {
  return base::ToVector(client_list, &ClientConnectionParameters::id);
}

}  // namespace

class SecureChannelActiveConnectionManagerImplTest : public testing::Test {
 public:
  SecureChannelActiveConnectionManagerImplTest(
      const SecureChannelActiveConnectionManagerImplTest&) = delete;
  SecureChannelActiveConnectionManagerImplTest& operator=(
      const SecureChannelActiveConnectionManagerImplTest&) = delete;

 protected:
  SecureChannelActiveConnectionManagerImplTest() = default;
  ~SecureChannelActiveConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_delegate_ = std::make_unique<FakeActiveConnectionManagerDelegate>();

    manager_ =
        ActiveConnectionManagerImpl::Factory::Create(fake_delegate_.get());

    ActiveConnectionManagerImpl* ptr_as_impl =
        static_cast<ActiveConnectionManagerImpl*>(manager_.get());
    fake_multiplexed_channel_factory_ =
        std::make_unique<FakeMultiplexedChannelFactory>(ptr_as_impl);
    MultiplexedChannelImpl::Factory::SetFactoryForTesting(
        fake_multiplexed_channel_factory_.get());
  }

  void TearDown() override {
    MultiplexedChannelImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void AddActiveConnectionAndVerifyState(
      const std::string& device_id,
      std::vector<std::unique_ptr<ClientConnectionParameters>>
          initial_clients) {
    EXPECT_EQ(ActiveConnectionManager::ConnectionState::kNoConnectionExists,
              GetConnectionState(device_id));

    std::vector<base::UnguessableToken> initial_client_ids =
        ClientListToIdList(initial_clients);

    auto fake_authenticated_channel =
        std::make_unique<FakeAuthenticatedChannel>();
    fake_multiplexed_channel_factory_->set_next_expected_authenticated_channel(
        fake_authenticated_channel.get());

    manager_->AddActiveConnection(
        std::move(fake_authenticated_channel), std::move(initial_clients),
        ConnectionDetails(device_id, ConnectionMedium::kBluetoothLowEnergy));

    // The connection should be active, and the initial clients should now be
    // present in the associated channel.
    EXPECT_EQ(ActiveConnectionManager::ConnectionState::kActiveConnectionExists,
              GetConnectionState(device_id));
    EXPECT_EQ(initial_client_ids,
              ClientListToIdList(
                  GetActiveChannelForDeviceId(device_id)->added_clients()));
  }

  void AddNewClientAndVerifyState(const std::string& device_id,
                                  std::unique_ptr<ClientConnectionParameters>
                                      client_connection_parameters) {
    EXPECT_EQ(ActiveConnectionManager::ConnectionState::kActiveConnectionExists,
              GetConnectionState(device_id));

    // Initialize to the IDs before this call.
    std::vector<base::UnguessableToken> client_ids = ClientListToIdList(
        GetActiveChannelForDeviceId(device_id)->added_clients());

    // Add in the new ID for this new client.
    client_ids.push_back(client_connection_parameters->id());

    manager_->AddClientToChannel(
        std::move(client_connection_parameters),
        ConnectionDetails(device_id, ConnectionMedium::kBluetoothLowEnergy));

    // The connection should remain active, and the clients list should now have
    // the new client.
    EXPECT_EQ(ActiveConnectionManager::ConnectionState::kActiveConnectionExists,
              GetConnectionState(device_id));
    EXPECT_EQ(client_ids,
              ClientListToIdList(
                  GetActiveChannelForDeviceId(device_id)->added_clients()));
  }

  ActiveConnectionManager::ConnectionState GetConnectionState(
      const std::string device_id) {
    return manager_->GetConnectionState(
        ConnectionDetails(device_id, ConnectionMedium::kBluetoothLowEnergy));
  }

  FakeMultiplexedChannel* GetActiveChannelForDeviceId(
      const std::string& device_id) {
    ConnectionDetails connection_details(device_id,
                                         ConnectionMedium::kBluetoothLowEnergy);
    if (!base::Contains(fake_multiplexed_channel_factory_
                            ->connection_details_to_active_channel_map(),
                        connection_details)) {
      return nullptr;
    }

    return fake_multiplexed_channel_factory_
        ->connection_details_to_active_channel_map()[connection_details];
  }

  size_t GetNumActiveChannels() {
    return fake_multiplexed_channel_factory_
        ->connection_details_to_active_channel_map()
        .size();
  }

  size_t GetNumDisconnections(const std::string& device_id) {
    ConnectionDetails connection_details(device_id,
                                         ConnectionMedium::kBluetoothLowEnergy);

    const auto& map =
        fake_delegate_->connection_details_to_num_disconnections_map();
    auto it = map.find(connection_details);
    EXPECT_NE(it, map.end());
    return it->second;
  }

  ActiveConnectionManager* active_connection_manager() {
    return manager_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeMultiplexedChannelFactory>
      fake_multiplexed_channel_factory_;
  std::unique_ptr<FakeActiveConnectionManagerDelegate> fake_delegate_;

  std::unique_ptr<ActiveConnectionManager> manager_;
};

TEST_F(SecureChannelActiveConnectionManagerImplTest, EdgeCases) {
  std::vector<std::unique_ptr<ClientConnectionParameters>> client_list;
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature"));

  AddActiveConnectionAndVerifyState("deviceId", std::move(client_list));

  // Try to add another channel for the same ConnectionDetails; this should
  // fail, since one already exists.
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature"));
  EXPECT_DCHECK_DEATH(active_connection_manager()->AddActiveConnection(
      std::make_unique<FakeAuthenticatedChannel>(), std::move(client_list),
      ConnectionDetails("deviceId", ConnectionMedium::kBluetoothLowEnergy)));

  // Move to disconnecting state.
  GetActiveChannelForDeviceId("deviceId")->SetDisconnecting();
  EXPECT_EQ(
      ActiveConnectionManager::ConnectionState::kDisconnectingConnectionExists,
      GetConnectionState("deviceId"));

  // Try to add another channel; this should still fail while disconnecting.
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature"));
  EXPECT_DCHECK_DEATH(active_connection_manager()->AddActiveConnection(
      std::make_unique<FakeAuthenticatedChannel>(), std::move(client_list),
      ConnectionDetails("deviceId", ConnectionMedium::kBluetoothLowEnergy)));

  // Try to add an additional client; this should also fail while disconnecting.
  EXPECT_DCHECK_DEATH(active_connection_manager()->AddClientToChannel(
      std::make_unique<FakeClientConnectionParameters>("feature"),
      ConnectionDetails("deviceId", ConnectionMedium::kBluetoothLowEnergy)));

  GetActiveChannelForDeviceId("deviceId")->SetDisconnected();
  EXPECT_EQ(ActiveConnectionManager::ConnectionState::kNoConnectionExists,
            GetConnectionState("deviceId"));

  // Try to add an additional client; this should also fail while disconnected.
  EXPECT_DCHECK_DEATH(active_connection_manager()->AddClientToChannel(
      std::make_unique<FakeClientConnectionParameters>("feature"),
      ConnectionDetails("deviceId", ConnectionMedium::kBluetoothLowEnergy)));
}

TEST_F(SecureChannelActiveConnectionManagerImplTest, SingleChannel_OneClient) {
  std::vector<std::unique_ptr<ClientConnectionParameters>> client_list;
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature"));

  AddActiveConnectionAndVerifyState("deviceId", std::move(client_list));
  EXPECT_EQ(1u, GetNumActiveChannels());

  GetActiveChannelForDeviceId("deviceId")->SetDisconnecting();
  EXPECT_EQ(
      ActiveConnectionManager::ConnectionState::kDisconnectingConnectionExists,
      GetConnectionState("deviceId"));
  EXPECT_EQ(1u, GetNumActiveChannels());

  GetActiveChannelForDeviceId("deviceId")->SetDisconnected();
  EXPECT_EQ(ActiveConnectionManager::ConnectionState::kNoConnectionExists,
            GetConnectionState("deviceId"));
  EXPECT_EQ(0u, GetNumActiveChannels());
  EXPECT_EQ(1u, GetNumDisconnections("deviceId"));
}

TEST_F(SecureChannelActiveConnectionManagerImplTest,
       SingleChannel_MultipleClients) {
  std::vector<std::unique_ptr<ClientConnectionParameters>> client_list;
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature1"));
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature2"));

  AddActiveConnectionAndVerifyState("deviceId", std::move(client_list));
  EXPECT_EQ(1u, GetNumActiveChannels());

  AddNewClientAndVerifyState(
      "deviceId", std::make_unique<FakeClientConnectionParameters>("feature3"));
  EXPECT_EQ(1u, GetNumActiveChannels());

  GetActiveChannelForDeviceId("deviceId")->SetDisconnecting();
  EXPECT_EQ(
      ActiveConnectionManager::ConnectionState::kDisconnectingConnectionExists,
      GetConnectionState("deviceId"));
  EXPECT_EQ(1u, GetNumActiveChannels());

  GetActiveChannelForDeviceId("deviceId")->SetDisconnected();
  EXPECT_EQ(ActiveConnectionManager::ConnectionState::kNoConnectionExists,
            GetConnectionState("deviceId"));
  EXPECT_EQ(0u, GetNumActiveChannels());
  EXPECT_EQ(1u, GetNumDisconnections("deviceId"));
}

TEST_F(SecureChannelActiveConnectionManagerImplTest,
       MultipleChannels_MultipleClients) {
  // Add an initial channel with two clients.
  std::vector<std::unique_ptr<ClientConnectionParameters>> client_list;
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature1"));
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature2"));

  AddActiveConnectionAndVerifyState("deviceId1", std::move(client_list));
  EXPECT_EQ(1u, GetNumActiveChannels());

  // Add another channel with two more clients.
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature3"));
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature4"));

  AddActiveConnectionAndVerifyState("deviceId2", std::move(client_list));
  EXPECT_EQ(2u, GetNumActiveChannels());

  // Add a new client to the first channel.
  AddNewClientAndVerifyState(
      "deviceId1",
      std::make_unique<FakeClientConnectionParameters>("feature5"));
  EXPECT_EQ(2u, GetNumActiveChannels());

  // Add a new client to the second channel.
  AddNewClientAndVerifyState(
      "deviceId2",
      std::make_unique<FakeClientConnectionParameters>("feature6"));
  EXPECT_EQ(2u, GetNumActiveChannels());

  // Start disconnecting the first channel.
  GetActiveChannelForDeviceId("deviceId1")->SetDisconnecting();
  EXPECT_EQ(
      ActiveConnectionManager::ConnectionState::kDisconnectingConnectionExists,
      GetConnectionState("deviceId1"));
  EXPECT_EQ(2u, GetNumActiveChannels());

  // Disconnect the first channel.
  GetActiveChannelForDeviceId("deviceId1")->SetDisconnected();
  EXPECT_EQ(ActiveConnectionManager::ConnectionState::kNoConnectionExists,
            GetConnectionState("deviceId1"));
  EXPECT_EQ(1u, GetNumActiveChannels());
  EXPECT_EQ(1u, GetNumDisconnections("deviceId1"));

  // Now, add another channel for the same device that just disconnected.
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature7"));
  client_list.push_back(
      std::make_unique<FakeClientConnectionParameters>("feature8"));

  AddActiveConnectionAndVerifyState("deviceId1", std::move(client_list));
  EXPECT_EQ(2u, GetNumActiveChannels());

  // Start disconnecting the second channel.
  GetActiveChannelForDeviceId("deviceId2")->SetDisconnecting();
  EXPECT_EQ(
      ActiveConnectionManager::ConnectionState::kDisconnectingConnectionExists,
      GetConnectionState("deviceId2"));
  EXPECT_EQ(2u, GetNumActiveChannels());

  // Disconnect the second channel.
  GetActiveChannelForDeviceId("deviceId2")->SetDisconnected();
  EXPECT_EQ(ActiveConnectionManager::ConnectionState::kNoConnectionExists,
            GetConnectionState("deviceId2"));
  EXPECT_EQ(1u, GetNumActiveChannels());
  EXPECT_EQ(1u, GetNumDisconnections("deviceId2"));

  // Start disconnecting the second iteration of the first channel.
  GetActiveChannelForDeviceId("deviceId1")->SetDisconnecting();
  EXPECT_EQ(
      ActiveConnectionManager::ConnectionState::kDisconnectingConnectionExists,
      GetConnectionState("deviceId1"));
  EXPECT_EQ(1u, GetNumActiveChannels());

  // Disconnect the second iteration of the first channel.
  GetActiveChannelForDeviceId("deviceId1")->SetDisconnected();
  EXPECT_EQ(ActiveConnectionManager::ConnectionState::kNoConnectionExists,
            GetConnectionState("deviceId1"));
  EXPECT_EQ(0u, GetNumActiveChannels());
  EXPECT_EQ(2u, GetNumDisconnections("deviceId1"));
}

}  // namespace ash::secure_channel
