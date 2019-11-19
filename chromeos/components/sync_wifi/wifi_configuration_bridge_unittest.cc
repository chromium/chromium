// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/wifi_configuration_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/synced_network_updater.h"
#include "chromeos/components/sync_wifi/test_data_generator.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace sync_wifi {

namespace {

using sync_pb::WifiConfigurationSpecificsData;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

const char kSsidMeow[] = "meow";
const char kSsidWoof[] = "woof";

std::unique_ptr<syncer::EntityData> GenerateWifiEntityData(
    const sync_pb::WifiConfigurationSpecificsData& data) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_wifi_configuration()
      ->mutable_client_only_encrypted_data()
      ->CopyFrom(data);
  entity_data->name = data.hex_ssid();
  return entity_data;
}

bool ProtoVectorContainsId(
    const std::vector<sync_pb::WifiConfigurationSpecificsData>& protos,
    NetworkIdentifier id) {
  return std::find_if(
             protos.begin(), protos.end(),
             [&id](const sync_pb::WifiConfigurationSpecificsData& specifics) {
               return NetworkIdentifier::FromProto(specifics) == id;
             }) != protos.end();
}

// Implementation of SyncedNetworkUpdater. This class takes add/update/delete
// network requests and stores them in its internal data structures without
// actually updating anything external.
class TestSyncedNetworkUpdater : public SyncedNetworkUpdater {
 public:
  TestSyncedNetworkUpdater() = default;
  ~TestSyncedNetworkUpdater() override = default;

  const std::vector<sync_pb::WifiConfigurationSpecificsData>&
  add_or_update_calls() {
    return add_update_calls_;
  }

  const std::vector<NetworkIdentifier>& remove_calls() { return remove_calls_; }

 private:
  void AddOrUpdateNetwork(
      const sync_pb::WifiConfigurationSpecificsData& specifics) override {
    add_update_calls_.push_back(specifics);
  }

  void RemoveNetwork(const NetworkIdentifier& id) override {
    remove_calls_.push_back(id);
  }

  std::vector<sync_pb::WifiConfigurationSpecificsData> add_update_calls_;
  std::vector<NetworkIdentifier> remove_calls_;
};

class WifiConfigurationBridgeTest : public testing::Test {
 protected:
  WifiConfigurationBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    synced_network_updater_ = std::make_unique<TestSyncedNetworkUpdater>();
    bridge_ = std::make_unique<WifiConfigurationBridge>(
        synced_network_updater(), mock_processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::MoveStoreToFactory(std::move(store_)));
  }

  void DisableBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(false));
  }

  syncer::EntityChangeList CreateEntityAddList(
      const std::vector<WifiConfigurationSpecificsData>& specifics_list) {
    syncer::EntityChangeList changes;
    for (const auto& data : specifics_list) {
      auto entity_data = std::make_unique<syncer::EntityData>();
      sync_pb::WifiConfigurationSpecifics specifics;

      specifics.mutable_client_only_encrypted_data()->CopyFrom(data);
      entity_data->specifics.mutable_wifi_configuration()->CopyFrom(specifics);

      entity_data->name = data.hex_ssid();

      changes.push_back(syncer::EntityChange::CreateAdd(
          data.hex_ssid(), std::move(entity_data)));
    }
    return changes;
  }

  syncer::MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  WifiConfigurationBridge* bridge() { return bridge_.get(); }

  TestSyncedNetworkUpdater* synced_network_updater() {
    return synced_network_updater_.get();
  }

  const NetworkIdentifier& woof_network_id() const { return woof_network_id_; }

  const NetworkIdentifier& meow_network_id() const { return meow_network_id_; }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<syncer::ModelTypeStore> store_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;

  std::unique_ptr<WifiConfigurationBridge> bridge_;

  std::unique_ptr<TestSyncedNetworkUpdater> synced_network_updater_;

  const NetworkIdentifier woof_network_id_ = GeneratePskNetworkId(kSsidWoof);

  const NetworkIdentifier meow_network_id_ = GeneratePskNetworkId(kSsidMeow);

  DISALLOW_COPY_AND_ASSIGN(WifiConfigurationBridgeTest);
};

TEST_F(WifiConfigurationBridgeTest, InitWithTwoNetworksFromServer) {
  syncer::EntityChangeList remote_input;

  WifiConfigurationSpecificsData entry1 =
      GenerateTestWifiSpecifics(meow_network_id());
  WifiConfigurationSpecificsData entry2 =
      GenerateTestWifiSpecifics(woof_network_id());

  remote_input.push_back(syncer::EntityChange::CreateAdd(
      meow_network_id().SerializeToString(), GenerateWifiEntityData(entry1)));
  remote_input.push_back(syncer::EntityChange::CreateAdd(
      woof_network_id().SerializeToString(), GenerateWifiEntityData(entry2)));

  bridge()->MergeSyncData(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(remote_input));

  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(2u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));
  EXPECT_TRUE(base::Contains(ids, woof_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));
  EXPECT_TRUE(ProtoVectorContainsId(networks, woof_network_id()));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesAddTwoSpecifics) {
  const WifiConfigurationSpecificsData specifics1 =
      GenerateTestWifiSpecifics(meow_network_id());
  const WifiConfigurationSpecificsData specifics2 =
      GenerateTestWifiSpecifics(woof_network_id());

  base::Optional<syncer::ModelError> error =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 CreateEntityAddList({specifics1, specifics2}));
  EXPECT_FALSE(error);
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(2u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));
  EXPECT_TRUE(base::Contains(ids, woof_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));
  EXPECT_TRUE(ProtoVectorContainsId(networks, woof_network_id()));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesOneAdd) {
  WifiConfigurationSpecificsData entry =
      GenerateTestWifiSpecifics(meow_network_id());

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      meow_network_id().SerializeToString(), GenerateWifiEntityData(entry)));

  bridge()->ApplySyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(add_changes));
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesOneDeletion) {
  WifiConfigurationSpecificsData entry =
      GenerateTestWifiSpecifics(meow_network_id());
  NetworkIdentifier id = NetworkIdentifier::FromProto(entry);

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      id.SerializeToString(), GenerateWifiEntityData(entry)));

  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(add_changes));
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete(id.SerializeToString()));

  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(delete_changes));
  EXPECT_TRUE(bridge()->GetAllIdsForTesting().empty());

  const std::vector<NetworkIdentifier>& removed_networks =
      synced_network_updater()->remove_calls();
  EXPECT_EQ(1u, removed_networks.size());
  EXPECT_EQ(removed_networks[0], id);
}

}  // namespace

}  // namespace sync_wifi

}  // namespace chromeos
