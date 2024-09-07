// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_sync_bridge.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/plus_address_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

using DataChangedBySyncCallbackMock =
    base::MockRepeatingCallback<void(std::vector<PlusAddressDataChange>)>;

class PlusAddressSyncBridgeTest : public testing::Test {
 public:
  PlusAddressSyncBridgeTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {
    // Table and bridge operate on the DB sequence - UI sequence doesn't matter.
    webdatabase_service_ = base::MakeRefCounted<WebDatabaseService>(
        base::FilePath(WebDatabase::kInMemoryPath),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    webdatabase_service_->AddTable(std::make_unique<PlusAddressTable>());
    webdatabase_service_->LoadDatabase(os_crypt_.get());
    task_environment_.RunUntilIdle();
    RecreateBridge();
  }

  // Simulates starting to sync with `remote_profiles` pre-existing on the
  // server-side. Returns true if syncing started successfully.
  bool StartSyncing(const std::vector<PlusProfile>& remote_profiles) {
    syncer::EntityChangeList change_list;
    for (const PlusProfile& profile : remote_profiles) {
      syncer::EntityData entity_data = EntityDataFromPlusProfile(profile);
      std::string storage_key = bridge().GetStorageKey(entity_data);
      change_list.push_back(
          syncer::EntityChange::CreateAdd(storage_key, std::move(entity_data)));
    }
    return !bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));
  }

  void RecreateBridge() {
    bridge_ = std::make_unique<PlusAddressSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        webdatabase_service_->GetBackend(), on_data_changed_callback_.Get());
  }

  PlusAddressSyncBridge& bridge() { return *bridge_; }

  PlusAddressTable& table() {
    return *PlusAddressTable::FromWebDatabase(
        webdatabase_service_->GetBackend()->database());
  }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  // Called by the sync bridge whenever it modifies data in `table()`.
  DataChangedBySyncCallbackMock& on_data_changed_callback() {
    return on_data_changed_callback_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  scoped_refptr<WebDatabaseService> webdatabase_service_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  testing::NiceMock<DataChangedBySyncCallbackMock> on_data_changed_callback_;
  std::unique_ptr<PlusAddressSyncBridge> bridge_;
};

// Tests that during the initial sync, when no metadata is stored yet,
// `ModelReadyToSync()` is called.
TEST_F(PlusAddressSyncBridgeTest, ModelReadyToSync_InitialSync) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync);
  RecreateBridge();
}

TEST_F(PlusAddressSyncBridgeTest, ModelReadyToSync_ExistingMetadata) {
  // Simulate that some metadata is stored.
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState::INITIAL_SYNC_DONE);
  ASSERT_TRUE(
      table().UpdateDataTypeState(syncer::PLUS_ADDRESS, data_type_state));

  // Expect that `ModelReadyToSync()` is called with the stored metadata when
  // the bridge is created.
  EXPECT_CALL(mock_processor(), ModelReadyToSync(syncer::MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/IsEmpty())));
  RecreateBridge();
}

TEST_F(PlusAddressSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData entity;
  sync_pb::PlusAddressSpecifics* specifics =
      entity.specifics.mutable_plus_address();
  // Missing a profile ID and facet.
  EXPECT_FALSE(bridge().IsEntityDataValid(entity));
  specifics->set_profile_id("123");
  specifics->set_facet("invalid facet");
  EXPECT_FALSE(bridge().IsEntityDataValid(entity));
  specifics->set_facet("https://test.example");
  EXPECT_TRUE(bridge().IsEntityDataValid(entity));
}

TEST_F(PlusAddressSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_plus_address()->set_profile_id("123");
  EXPECT_EQ(bridge().GetStorageKey(entity), "123");
}

TEST_F(PlusAddressSyncBridgeTest, MergeFullSyncData) {
  const PlusProfile profile1 = test::CreatePlusProfile();
  const PlusProfile profile2 = test::CreatePlusProfile2();
  EXPECT_CALL(
      on_data_changed_callback(),
      Run(/*changes=*/ElementsAre(
          PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile1),
          PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile2))));
  EXPECT_TRUE(StartSyncing(/*remote_profiles=*/{profile1, profile2}));
  EXPECT_THAT(table().GetPlusProfiles(),
              UnorderedElementsAre(profile1, profile2));
}

TEST_F(PlusAddressSyncBridgeTest, ApplyIncrementalSyncChanges_AddUpdate) {
  PlusProfile profile1 = test::CreatePlusProfile();
  EXPECT_CALL(on_data_changed_callback(),
              Run(/*changes=*/UnorderedElementsAre(PlusAddressDataChange(
                  PlusAddressDataChange::Type::kAdd, profile1))));
  ASSERT_TRUE(StartSyncing(/*remote_profiles=*/{profile1}));

  // Simulate receiving an incremental update.
  // Update `profile1`.
  syncer::EntityChangeList change_list;
  PlusProfile old_profile1 = profile1;
  profile1.plus_address = PlusAddress("new-" + *profile1.plus_address);
  syncer::EntityData entity_data = EntityDataFromPlusProfile(profile1);
  std::string storage_key = bridge().GetStorageKey(entity_data);
  change_list.push_back(
      syncer::EntityChange::CreateUpdate(storage_key, std::move(entity_data)));
  // Add `profile2`.
  const PlusProfile profile2 = test::CreatePlusProfile2();
  entity_data = EntityDataFromPlusProfile(profile2);
  storage_key = bridge().GetStorageKey(entity_data);
  change_list.push_back(
      syncer::EntityChange::CreateAdd(storage_key, std::move(entity_data)));
  // `ApplyIncrementalSyncChanges()` returns an error if it fails.
  EXPECT_CALL(
      on_data_changed_callback(),
      Run(/*changes=*/ElementsAre(
          PlusAddressDataChange(PlusAddressDataChange::Type::kRemove,
                                old_profile1),
          PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile1),
          PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile2))));
  EXPECT_FALSE(bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(change_list)));

  EXPECT_THAT(table().GetPlusProfiles(),
              UnorderedElementsAre(profile1, profile2));
}

TEST_F(PlusAddressSyncBridgeTest, ApplyIncrementalSyncChanges_Remove) {
  const PlusProfile profile = test::CreatePlusProfile();
  ASSERT_TRUE(StartSyncing(/*remote_profiles=*/{profile}));

  // Simulate receiving an incremental update removing `profile1`.
  syncer::EntityChangeList change_list;
  change_list.push_back(syncer::EntityChange::CreateDelete(
      bridge().GetStorageKey(EntityDataFromPlusProfile(profile))));
  // `ApplyIncrementalSyncChanges()` returns an error if it fails.
  EXPECT_CALL(on_data_changed_callback(),
              Run(/*changes=*/ElementsAre(PlusAddressDataChange(
                  PlusAddressDataChange::Type::kRemove, profile))));
  EXPECT_FALSE(bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(change_list)));

  EXPECT_THAT(table().GetPlusProfiles(), IsEmpty());
}

TEST_F(PlusAddressSyncBridgeTest, ApplyDisableSyncChanges) {
  const PlusProfile profile = test::CreatePlusProfile();
  ASSERT_TRUE(StartSyncing(/*remote_profiles=*/{profile}));
  EXPECT_CALL(on_data_changed_callback(),
              Run(/*changes=*/ElementsAre(PlusAddressDataChange(
                  PlusAddressDataChange::Type::kRemove, profile))));
  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  EXPECT_THAT(table().GetPlusProfiles(), IsEmpty());
}

TEST_F(PlusAddressSyncBridgeTest, GetAllDataForDebugging) {
  const PlusProfile profile1 = test::CreatePlusProfile();
  const PlusProfile profile2 = test::CreatePlusProfile2();
  ASSERT_TRUE(table().AddOrUpdatePlusProfile(profile1));
  ASSERT_TRUE(table().AddOrUpdatePlusProfile(profile2));

  std::unique_ptr<syncer::DataBatch> batch = bridge().GetAllDataForDebugging();
  std::vector<PlusProfile> profiles_from_batch;
  while (batch->HasNext()) {
    profiles_from_batch.push_back(
        PlusProfileFromEntityData(*batch->Next().second));
  }
  EXPECT_THAT(profiles_from_batch, UnorderedElementsAre(profile1, profile2));
}

}  // namespace

}  // namespace plus_addresses
