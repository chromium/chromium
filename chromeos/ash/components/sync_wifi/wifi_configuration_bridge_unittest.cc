// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/wifi_configuration_bridge.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/fake_local_network_collector.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/ash/components/sync_wifi/synced_network_updater.h"
#include "chromeos/ash/components/sync_wifi/test_data_generator.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::sync_wifi {

namespace {

using sync_pb::WifiConfigurationSpecifics;
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
const char kSsidHonk[] = "honk";
const char kSyncPsk[] = "sync_psk";
const char kLocalPsk[] = "local_psk";

syncer::EntityData GenerateWifiEntityData(
    const sync_pb::WifiConfigurationSpecifics& data) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_wifi_configuration()->CopyFrom(data);
  entity_data.name = data.hex_ssid();
  return entity_data;
}

bool VectorContainsProto(
    const std::vector<sync_pb::WifiConfigurationSpecifics>& protos,
    const sync_pb::WifiConfigurationSpecifics& proto) {
  return base::ranges::any_of(
      protos, [&proto](const sync_pb::WifiConfigurationSpecifics& specifics) {
        return NetworkIdentifier::FromProto(specifics) ==
                   NetworkIdentifier::FromProto(proto) &&
               specifics.last_connected_timestamp() ==
                   proto.last_connected_timestamp() &&
               specifics.passphrase() == proto.passphrase();
      });
}

void ExtractProtosFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch,
    std::vector<sync_pb::WifiConfigurationSpecifics>* output) {
  while (batch->HasNext()) {
    const auto& [key, data] = batch->Next();
    output->push_back(data->specifics.wifi_configuration());
  }
}

// Implementation of SyncedNetworkUpdater. This class takes add/update/delete
// network requests and stores them in its internal data structures without
// actually updating anything external.
class TestSyncedNetworkUpdater : public SyncedNetworkUpdater {
 public:
  TestSyncedNetworkUpdater() = default;
  ~TestSyncedNetworkUpdater() override = default;

  const std::vector<sync_pb::WifiConfigurationSpecifics>&
  add_or_update_calls() {
    return add_update_calls_;
  }

  const std::vector<NetworkIdentifier>& remove_calls() { return remove_calls_; }

  void set_update_in_progress(const std::string& network_guid, bool value) {
    if (value)
      guid_update_in_progress_.insert(network_guid);
    else
      guid_update_in_progress_.erase(network_guid);
  }

  // SyncedNetworkUpdater:
  void AddOrUpdateNetwork(
      const sync_pb::WifiConfigurationSpecifics& specifics) override {
    add_update_calls_.push_back(specifics);
  }

  void RemoveNetwork(const NetworkIdentifier& id) override {
    remove_calls_.push_back(id);
  }

  bool IsUpdateInProgress(const std::string& network_guid) override {
    return guid_update_in_progress_.contains(network_guid);
  }

 private:
  std::vector<sync_pb::WifiConfigurationSpecifics> add_update_calls_;
  std::vector<NetworkIdentifier> remove_calls_;
  base::flat_set<std::string> guid_update_in_progress_;
};

class WifiConfigurationBridgeTest : public testing::Test {
 public:
  WifiConfigurationBridgeTest(const WifiConfigurationBridgeTest&) = delete;
  WifiConfigurationBridgeTest& operator=(const WifiConfigurationBridgeTest&) =
      delete;

 protected:
  WifiConfigurationBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    network_test_helper_ = std::make_unique<NetworkTestHelper>();
  }

  void SetUp() override {
    network_test_helper_->SetUp();

    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    timer_factory_ = std::make_unique<ash::timer_factory::FakeTimerFactory>();
    synced_network_updater_ = std::make_unique<TestSyncedNetworkUpdater>();
    local_network_collector_ = std::make_unique<FakeLocalNetworkCollector>();
    metrics_logger_ = std::make_unique<SyncedNetworkMetricsLogger>(
        /*network_state_handler=*/nullptr,
        /*network_connection_handler=*/nullptr);

    WifiConfigurationBridge::RegisterPrefs(
        network_test_helper_->user_prefs()->registry());

    network_metadata_store_ = NetworkHandler::Get()->network_metadata_store();

    base::HistogramTester histogram_tester;
    bridge_ = std::make_unique<WifiConfigurationBridge>(
        synced_network_updater(), local_network_collector(),
        /*network_configuration_handler=*/nullptr, metrics_logger_.get(),
        timer_factory_.get(), network_test_helper_->user_prefs(),
        mock_processor_.CreateForwardingProcessor(),
        CreateDelayedStoreCallback());
    bridge_->SetNetworkMetadataStore(network_metadata_store_->GetWeakPtr());

    // Assert that an incorrect metric was not logged.
    histogram_tester.ExpectTotalCount(kTotalCountHistogram, 0);
  }

  syncer::OnceDataTypeStoreFactory CreateDelayedStoreCallback() {
    return base::BindOnce(&WifiConfigurationBridgeTest::OnDataTypeStoreCallback,
                          base::Unretained(this));
  }

  void InitializeSyncStore() {
    std::move(init_callback_).Run(/*error=*/std::nullopt, std::move(store_));
    base::RunLoop().RunUntilIdle();
  }

  void OnDataTypeStoreCallback(syncer::DataType type,
                               syncer::DataTypeStore::InitCallback callback) {
    init_callback_ = std::move(callback);
  }

  void DisableBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(false));
  }

  syncer::EntityChangeList CreateEntityAddList(
      const std::vector<WifiConfigurationSpecifics>& specifics_list) {
    syncer::EntityChangeList changes;
    for (const auto& proto : specifics_list) {
      syncer::EntityData entity_data;
      entity_data.specifics.mutable_wifi_configuration()->CopyFrom(proto);
      entity_data.name = proto.hex_ssid();

      changes.push_back(syncer::EntityChange::CreateAdd(
          proto.hex_ssid(), std::move(entity_data)));
    }
    return changes;
  }

  std::vector<sync_pb::WifiConfigurationSpecifics> GetAllSyncedData() {
    std::vector<WifiConfigurationSpecifics> data;
    ExtractProtosFromDataBatch(bridge()->GetAllDataForDebugging(), &data);
    return data;
  }

  // This can only be called before InitializeSyncStore().
  void PresaveSyncedNetwork(const WifiConfigurationSpecifics& proto) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    std::string storage_key =
        NetworkIdentifier::FromProto(proto).SerializeToString();
    batch->WriteData(storage_key, proto.SerializeAsString());

    base::RunLoop run_loop;
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindLambdaForTesting(
            [&](const std::optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  syncer::MockDataTypeLocalChangeProcessor* processor() {
    return &mock_processor_;
  }

  WifiConfigurationBridge* bridge() { return bridge_.get(); }

  TestSyncedNetworkUpdater* synced_network_updater() {
    return synced_network_updater_.get();
  }

  FakeLocalNetworkCollector* local_network_collector() {
    return local_network_collector_.get();
  }

  ash::timer_factory::FakeTimerFactory* timer_factory() {
    return timer_factory_.get();
  }
  NetworkMetadataStore* network_metadata_store() {
    return network_metadata_store_;
  }
  NetworkTestHelper* network_test_helper() {
    return network_test_helper_.get();
  }

  const NetworkIdentifier& woof_network_id() const { return woof_network_id_; }
  const NetworkIdentifier& meow_network_id() const { return meow_network_id_; }
  const NetworkIdentifier& honk_network_id() const { return honk_network_id_; }

 private:
  base::test::TaskEnvironment task_environment_;
  syncer::DataTypeStore::InitCallback init_callback_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<WifiConfigurationBridge> bridge_;
  std::unique_ptr<TestSyncedNetworkUpdater> synced_network_updater_;
  std::unique_ptr<FakeLocalNetworkCollector> local_network_collector_;
  std::unique_ptr<ash::timer_factory::FakeTimerFactory> timer_factory_;
  std::unique_ptr<TestingPrefServiceSimple> device_prefs_;
  std::unique_ptr<SyncedNetworkMetricsLogger> metrics_logger_;
  std::unique_ptr<NetworkTestHelper> network_test_helper_;
  raw_ptr<NetworkMetadataStore> network_metadata_store_;

  const NetworkIdentifier woof_network_id_ = GeneratePskNetworkId(kSsidWoof);
  const NetworkIdentifier meow_network_id_ = GeneratePskNetworkId(kSsidMeow);
  const NetworkIdentifier honk_network_id_ = GeneratePskNetworkId(kSsidHonk);
};

TEST_F(WifiConfigurationBridgeTest, InitWithTwoNetworksFromServer) {
  base::HistogramTester histogram_tester;
  syncer::EntityChangeList remote_input;

  InitializeSyncStore();

  WifiConfigurationSpecifics meow_network =
      GenerateTestWifiSpecifics(meow_network_id());
  WifiConfigurationSpecifics woof_network =
      GenerateTestWifiSpecifics(woof_network_id());

  remote_input.push_back(
      syncer::EntityChange::CreateAdd(meow_network_id().SerializeToString(),
                                      GenerateWifiEntityData(meow_network)));
  remote_input.push_back(
      syncer::EntityChange::CreateAdd(woof_network_id().SerializeToString(),
                                      GenerateWifiEntityData(woof_network)));

  bridge()->MergeFullSyncData(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(remote_input));

  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(2u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));
  EXPECT_TRUE(base::Contains(ids, woof_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(VectorContainsProto(networks, meow_network));
  EXPECT_TRUE(VectorContainsProto(networks, woof_network));
  histogram_tester.ExpectTotalCount(kTotalCountHistogram, 1);
}

TEST_F(WifiConfigurationBridgeTest,
       ApplyIncrementalSyncChangesAddTwoSpecifics) {
  InitializeSyncStore();

  const WifiConfigurationSpecifics meow_network =
      GenerateTestWifiSpecifics(meow_network_id());
  const WifiConfigurationSpecifics woof_network =
      GenerateTestWifiSpecifics(woof_network_id());

  std::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(),
          CreateEntityAddList({meow_network, woof_network}));
  EXPECT_FALSE(error);
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(2u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));
  EXPECT_TRUE(base::Contains(ids, woof_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(VectorContainsProto(networks, woof_network));
  EXPECT_TRUE(VectorContainsProto(networks, meow_network));
}

TEST_F(WifiConfigurationBridgeTest, ApplyIncrementalSyncChangesOneAdd) {
  InitializeSyncStore();

  WifiConfigurationSpecifics entry =
      GenerateTestWifiSpecifics(meow_network_id());

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      meow_network_id().SerializeToString(), GenerateWifiEntityData(entry)));

  bridge()->ApplyIncrementalSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(add_changes));
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(VectorContainsProto(networks, entry));
}

TEST_F(WifiConfigurationBridgeTest,
       ApplyIncrementalSyncChangesOneDeletion_DeletesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWifiSyncApplyDeletes);
  InitializeSyncStore();

  WifiConfigurationSpecifics entry =
      GenerateTestWifiSpecifics(meow_network_id());
  NetworkIdentifier id = NetworkIdentifier::FromProto(entry);

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      id.SerializeToString(), GenerateWifiEntityData(entry)));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(add_changes));
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(VectorContainsProto(networks, entry));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete(id.SerializeToString()));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(delete_changes));
  EXPECT_TRUE(bridge()->GetAllIdsForTesting().empty());

  const std::vector<NetworkIdentifier>& removed_networks =
      synced_network_updater()->remove_calls();
  EXPECT_TRUE(removed_networks.empty());
}

TEST_F(WifiConfigurationBridgeTest,
       ApplyIncrementalSyncChangesOneDeletion_DeletesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWifiSyncApplyDeletes);
  InitializeSyncStore();

  WifiConfigurationSpecifics entry =
      GenerateTestWifiSpecifics(meow_network_id());
  NetworkIdentifier id = NetworkIdentifier::FromProto(entry);

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      id.SerializeToString(), GenerateWifiEntityData(entry)));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(add_changes));
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(VectorContainsProto(networks, entry));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete(id.SerializeToString()));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(delete_changes));
  EXPECT_TRUE(bridge()->GetAllIdsForTesting().empty());

  const std::vector<NetworkIdentifier>& removed_networks =
      synced_network_updater()->remove_calls();
  EXPECT_EQ(1u, removed_networks.size());
  EXPECT_EQ(removed_networks[0], id);
}

TEST_F(WifiConfigurationBridgeTest, MergeFullSyncData) {
  InitializeSyncStore();

  base::HistogramTester histogram_tester;
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  syncer::EntityChangeList entity_data;

  WifiConfigurationSpecifics meow_sync =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  WifiConfigurationSpecifics woof_sync =
      GenerateTestWifiSpecifics(woof_network_id(), kSyncPsk, /*timestamp=*/100);
  WifiConfigurationSpecifics honk_sync =
      GenerateTestWifiSpecifics(honk_network_id(), kSyncPsk, /*timestamp=*/100);
  entity_data.push_back(
      syncer::EntityChange::CreateAdd(meow_network_id().SerializeToString(),
                                      GenerateWifiEntityData(meow_sync)));
  entity_data.push_back(
      syncer::EntityChange::CreateAdd(woof_network_id().SerializeToString(),
                                      GenerateWifiEntityData(woof_sync)));
  entity_data.push_back(
      syncer::EntityChange::CreateAdd(honk_network_id().SerializeToString(),
                                      GenerateWifiEntityData(honk_sync)));

  WifiConfigurationSpecifics woof_local =
      GenerateTestWifiSpecifics(woof_network_id(), kLocalPsk, /*timestamp=*/1);
  WifiConfigurationSpecifics meow_local = GenerateTestWifiSpecifics(
      meow_network_id(), kLocalPsk, /*timestamp=*/1000);
  local_network_collector()->AddNetwork(woof_local);
  local_network_collector()->AddNetwork(meow_local);

  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(testing::SaveArg<0>(&storage_key));

  bridge()->MergeFullSyncData(std::move(metadata_change_list),
                              std::move(entity_data));
  base::RunLoop().RunUntilIdle();

  // Verify local network was added to sync.
  EXPECT_EQ(storage_key, meow_network_id().SerializeToString());

  // Verify sync network was added to local stack.
  const std::vector<sync_pb::WifiConfigurationSpecifics>&
      updated_local_networks = synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, updated_local_networks.size());
  EXPECT_TRUE(VectorContainsProto(updated_local_networks, woof_sync));
  EXPECT_TRUE(VectorContainsProto(updated_local_networks, honk_sync));

  std::vector<sync_pb::WifiConfigurationSpecifics> sync_networks =
      GetAllSyncedData();
  EXPECT_EQ(3u, sync_networks.size());
  EXPECT_TRUE(VectorContainsProto(sync_networks, meow_local));
  EXPECT_TRUE(VectorContainsProto(sync_networks, woof_sync));
  EXPECT_TRUE(VectorContainsProto(sync_networks, honk_sync));
  histogram_tester.ExpectTotalCount(kTotalCountHistogram, 1);
}

TEST_F(WifiConfigurationBridgeTest,
       ApplyDisableSyncChangesAndMergeFullSyncData) {
  InitializeSyncStore();

  // Mimic initial sync with single sync network.
  auto metadata_change_list1 =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  syncer::EntityChangeList entity_data1;

  WifiConfigurationSpecifics meow_sync =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  entity_data1.push_back(
      syncer::EntityChange::CreateAdd(meow_network_id().SerializeToString(),
                                      GenerateWifiEntityData(meow_sync)));
  bridge()->MergeFullSyncData(std::move(metadata_change_list1),
                              std::move(entity_data1));
  base::RunLoop().RunUntilIdle();

  // Verify sync network was added to local stack.
  const std::vector<sync_pb::WifiConfigurationSpecifics>&
      updated_local_networks = synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, updated_local_networks.size());
  EXPECT_TRUE(VectorContainsProto(updated_local_networks, meow_sync));

  // Mimic sync being stopped with request to clear metadata.
  bridge()->ApplyDisableSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>());

  // Add local network while sync is not running.
  WifiConfigurationSpecifics woof_local =
      GenerateTestWifiSpecifics(woof_network_id(), kLocalPsk, /*timestamp=*/1);
  local_network_collector()->AddNetwork(woof_local);

  // Add sync network while sync is not running.
  auto metadata_change_list2 =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  auto entity_data2 = syncer::EntityChangeList();
  WifiConfigurationSpecifics honk_sync =
      GenerateTestWifiSpecifics(honk_network_id(), kSyncPsk, /*timestamp=*/100);
  entity_data2.push_back(
      syncer::EntityChange::CreateAdd(meow_network_id().SerializeToString(),
                                      GenerateWifiEntityData(meow_sync)));
  entity_data2.push_back(
      syncer::EntityChange::CreateAdd(honk_network_id().SerializeToString(),
                                      GenerateWifiEntityData(honk_sync)));

  // Mimic sync restart and trigger initial sync.
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(testing::SaveArg<0>(&storage_key));

  bridge()->MergeFullSyncData(std::move(metadata_change_list2),
                              std::move(entity_data2));
  base::RunLoop().RunUntilIdle();

  // Verify local network was added to sync.
  EXPECT_EQ(storage_key, woof_network_id().SerializeToString());

  // Verify local state.
  std::vector<sync_pb::WifiConfigurationSpecifics> sync_networks =
      GetAllSyncedData();
  EXPECT_EQ(3u, sync_networks.size());
  EXPECT_TRUE(VectorContainsProto(sync_networks, meow_sync));
  EXPECT_TRUE(VectorContainsProto(sync_networks, woof_local));
  EXPECT_TRUE(VectorContainsProto(sync_networks, honk_sync));
}

TEST_F(WifiConfigurationBridgeTest, LocalConfigured) {
  InitializeSyncStore();

  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/0);
  local_network_collector()->AddNetwork(meow_local);

  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(testing::SaveArg<0>(&storage_key));
  std::string guid = meow_network_id().SerializeToString();
  bridge()->OnNetworkCreated(guid);
  base::RunLoop().RunUntilIdle();

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiConfigurationBridgeTest, LocalConfigured_BeforeInit) {
  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/0);
  local_network_collector()->AddNetwork(meow_local);

  EXPECT_CALL(*processor(), Put).Times(0);
  std::string guid = meow_network_id().SerializeToString();
  bridge()->OnNetworkCreated(guid);
  base::RunLoop().RunUntilIdle();

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*processor(), Put).Times(1);
  InitializeSyncStore();
}

TEST_F(WifiConfigurationBridgeTest, LocalConfiguredAndUpdated_BeforeInit) {
  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/0);
  local_network_collector()->AddNetwork(meow_local);

  EXPECT_CALL(*processor(), Put).Times(0);
  std::string guid = meow_network_id().SerializeToString();
  bridge()->OnNetworkCreated(guid);
  base::RunLoop().RunUntilIdle();

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();

  meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  local_network_collector()->AddNetwork(meow_local);

  base::Value::Dict set_properties;
  set_properties.Set(shill::kAutoConnectProperty, true);
  bridge()->OnNetworkUpdate(guid, &set_properties);

  // Only the last change for a network is synced.
  EXPECT_CALL(*processor(), Put).Times(1);
  InitializeSyncStore();
}

TEST_F(WifiConfigurationBridgeTest, LocalConfigured_BadPassword) {
  InitializeSyncStore();

  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/0);

  std::string storage_key;
  EXPECT_CALL(*processor(), Put).Times(0);

  std::string guid = meow_network_id().SerializeToString();
  bridge()->OnNetworkCreated(guid);
  base::RunLoop().RunUntilIdle();

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiConfigurationBridgeTest, LocalConfigured_FromSync) {
  InitializeSyncStore();

  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/0);
  local_network_collector()->AddNetwork(meow_local);

  EXPECT_CALL(*processor(), Put).Times(0);

  std::string guid = meow_network_id().SerializeToString();
  bridge()->OnNetworkCreated(guid);
  network_metadata_store()->SetIsConfiguredBySync(guid);
  base::RunLoop().RunUntilIdle();

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiConfigurationBridgeTest, LocalFirstConnect) {
  InitializeSyncStore();

  base::HistogramTester histogram_tester;
  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  local_network_collector()->AddNetwork(meow_local);

  std::string storage_key;
  EXPECT_CALL(*processor(), Put)
      .WillOnce(testing::SaveArg<0>(&storage_key));
  bridge()->OnFirstConnectionToNetwork(meow_network_id().SerializeToString());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(storage_key, meow_network_id().SerializeToString());
  histogram_tester.ExpectTotalCount(kTotalCountHistogram, 1);
}

TEST_F(WifiConfigurationBridgeTest, LocalUpdate) {
  InitializeSyncStore();

  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  local_network_collector()->AddNetwork(meow_local);

  std::string storage_key;
  EXPECT_CALL(*processor(), Put)
      .WillOnce(testing::SaveArg<0>(&storage_key));
  std::string guid = meow_network_id().SerializeToString();
  base::Value::Dict set_properties;
  set_properties.Set(shill::kAutoConnectProperty, true);
  bridge()->OnNetworkUpdate(guid, &set_properties);
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiConfigurationBridgeTest, LocalUpdate_UntrackedField) {
  InitializeSyncStore();

  base::HistogramTester histogram_tester;
  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  local_network_collector()->AddNetwork(meow_local);

  EXPECT_CALL(*processor(), Put).Times(0);
  std::string guid = meow_network_id().SerializeToString();
  base::Value::Dict set_properties;
  set_properties.Set(shill::kUIDataProperty, "random_change");
  bridge()->OnNetworkUpdate(guid, &set_properties);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kTotalCountHistogram, 0);
}

TEST_F(WifiConfigurationBridgeTest, LocalUpdate_FromSync) {
  InitializeSyncStore();

  base::HistogramTester histogram_tester;
  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  std::string guid = meow_network_id().SerializeToString();
  local_network_collector()->AddNetwork(meow_local);
  synced_network_updater()->set_update_in_progress(guid, true);

  EXPECT_CALL(*processor(), Put).Times(0);

  base::Value::Dict set_properties;
  set_properties.Set(shill::kAutoConnectProperty, true);
  bridge()->OnNetworkUpdate(guid, &set_properties);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kTotalCountHistogram, 0);
}

TEST_F(WifiConfigurationBridgeTest, LocalRemove_DeletesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWifiSyncAllowDeletes);
  InitializeSyncStore();

  base::HistogramTester histogram_tester;
  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  local_network_collector()->AddNetwork(meow_local);
  std::string guid = meow_network_id().SerializeToString();

  bridge()->OnFirstConnectionToNetwork(guid);
  base::RunLoop().RunUntilIdle();

  bridge()->OnBeforeConfigurationRemoved("service_path", guid);

  EXPECT_CALL(*processor(), Delete).Times(0);
  bridge()->OnConfigurationRemoved("service_path", guid);
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiConfigurationBridgeTest, LocalRemove_DeletesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWifiSyncAllowDeletes);
  InitializeSyncStore();

  base::HistogramTester histogram_tester;
  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  local_network_collector()->AddNetwork(meow_local);
  std::string guid = meow_network_id().SerializeToString();

  bridge()->OnFirstConnectionToNetwork(guid);
  base::RunLoop().RunUntilIdle();

  bridge()->OnBeforeConfigurationRemoved("service_path", guid);

  std::string storage_key;
  EXPECT_CALL(*processor(), Delete(_, _, _))
      .WillOnce(testing::SaveArg<0>(&storage_key));
  bridge()->OnConfigurationRemoved("service_path", guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(storage_key, meow_network_id().SerializeToString());
  histogram_tester.ExpectTotalCount(kTotalCountHistogram, 1);
}

TEST_F(WifiConfigurationBridgeTest, LocalRemoved_BeforeInit_DeletesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWifiSyncAllowDeletes);

  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  std::string guid = meow_network_id().SerializeToString();
  local_network_collector()->AddNetwork(meow_local);
  PresaveSyncedNetwork(meow_local);
  bridge()->OnBeforeConfigurationRemoved("service_path", guid);

  EXPECT_CALL(*processor(), Delete).Times(0);
  bridge()->OnConfigurationRemoved("service_path", guid);
  base::RunLoop().RunUntilIdle();

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*processor(), Delete).Times(0);
  InitializeSyncStore();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiConfigurationBridgeTest, LocalRemoved_BeforeInit_DeletesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWifiSyncAllowDeletes);

  WifiConfigurationSpecifics meow_local =
      GenerateTestWifiSpecifics(meow_network_id(), kSyncPsk, /*timestamp=*/100);
  std::string guid = meow_network_id().SerializeToString();
  local_network_collector()->AddNetwork(meow_local);
  PresaveSyncedNetwork(meow_local);
  bridge()->OnBeforeConfigurationRemoved("service_path", guid);

  EXPECT_CALL(*processor(), Delete).Times(0);
  bridge()->OnConfigurationRemoved("service_path", guid);
  base::RunLoop().RunUntilIdle();

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*processor(), Delete).Times(1);
  InitializeSyncStore();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiConfigurationBridgeTest, FixAutoconnect) {
  EXPECT_FALSE(local_network_collector()->has_fixed_autoconnect());

  InitializeSyncStore();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(local_network_collector()->has_fixed_autoconnect());
}

TEST_F(WifiConfigurationBridgeTest, FixAutoconnect_AlreadyDone) {
  network_test_helper()->user_prefs()->SetBoolean(kHasFixedAutoconnect, true);

  EXPECT_FALSE(local_network_collector()->has_fixed_autoconnect());

  InitializeSyncStore();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(local_network_collector()->has_fixed_autoconnect());
}
}  // namespace

}  // namespace ash::sync_wifi
