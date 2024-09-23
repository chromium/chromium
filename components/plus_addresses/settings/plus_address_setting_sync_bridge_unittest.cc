// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_test_util.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

using SettingSpecifics = sync_pb::PlusAddressSettingSpecifics;
using ::testing::_;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

syncer::EntityData EntityFromSpecifics(const SettingSpecifics& specifics) {
  syncer::EntityData entity;
  *entity.specifics.mutable_plus_address_setting() = specifics;
  return entity;
}

// Assumes that `batch` only contains entities with `SettingSpecifics` and
// extracts them into a vector.
std::vector<SettingSpecifics> ExtractSpecificsFromBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<SettingSpecifics> specifics;
  while (batch->HasNext()) {
    std::unique_ptr<syncer::EntityData> entity = batch->Next().second;
    CHECK(entity->specifics.has_plus_address_setting());
    specifics.push_back(entity->specifics.plus_address_setting());
  }
  return specifics;
}

class PlusAddressSettingSyncBridgeTest : public testing::Test {
 public:
  PlusAddressSettingSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    RecreateBridge();
  }

  void RecreateBridge() {
    bridge_ = std::make_unique<PlusAddressSettingSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
    // Even though the test uses an in-memory store, it still posts tasks. Wait
    // for initialisation to complete.
    task_environment_.RunUntilIdle();
  }

  PlusAddressSettingSyncBridge& bridge() { return *bridge_; }

  syncer::DataTypeStore& store() { return *store_; }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  // Simulates starting to sync with `remote_specifics` pre-existing on the
  // server-side. Returns true if syncing started successfully.
  bool StartSyncingWithServerData(
      const std::vector<SettingSpecifics>& remote_specifics) {
    syncer::EntityChangeList change_list;
    for (const SettingSpecifics& specifics : remote_specifics) {
      change_list.push_back(syncer::EntityChange::CreateAdd(
          specifics.name(), EntityFromSpecifics(specifics)));
    }
    // `MergeFullSyncData()` returns an error if it fails.
    const bool success = !bridge().MergeFullSyncData(
        bridge().CreateMetadataChangeList(), std::move(change_list));
    ON_CALL(mock_processor(), IsTrackingMetadata)
        .WillByDefault(testing::Return(success));
    return success;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<PlusAddressSettingSyncBridge> bridge_;
};

TEST_F(PlusAddressSettingSyncBridgeTest, ModelReadyToSync_InitialSync) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync);
  RecreateBridge();
}

TEST_F(PlusAddressSettingSyncBridgeTest, ModelReadyToSync_ExistingMetadata) {
  // Simulate that some metadata is stored.
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState::INITIAL_SYNC_DONE);
  auto write_batch = store().CreateWriteBatch();
  write_batch->GetMetadataChangeList()->UpdateDataTypeState(data_type_state);
  base::test::TestFuture<const std::optional<syncer::ModelError>&> write_result;
  store().CommitWriteBatch(std::move(write_batch), write_result.GetCallback());
  ASSERT_FALSE(write_result.Get());

  // Expect that `ModelReadyToSync()` is called with the stored metadata when
  // the bridge is created.
  EXPECT_CALL(mock_processor(), ModelReadyToSync(syncer::MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/testing::IsEmpty())));
  RecreateBridge();
}

TEST_F(PlusAddressSettingSyncBridgeTest, IsEntityDataValid) {
  SettingSpecifics specifics = CreateSettingSpecifics("name", "value");
  EXPECT_TRUE(bridge().IsEntityDataValid(EntityFromSpecifics(specifics)));
  // Specifics with a missing name are invalid.
  specifics.clear_name();
  EXPECT_FALSE(bridge().IsEntityDataValid(EntityFromSpecifics(specifics)));
}

TEST_F(PlusAddressSettingSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity =
      EntityFromSpecifics(CreateSettingSpecifics("name", "value"));
  EXPECT_EQ(bridge().GetStorageKey(entity), "name");
  // `GetClientTag()` is implemented using `GetStorageKey()`.
  EXPECT_EQ(bridge().GetClientTag(entity), "name");
}

TEST_F(PlusAddressSettingSyncBridgeTest, MergeFullSyncData) {
  EXPECT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name", "value")}));
  // Expect that the setting is available immediately.
  EXPECT_THAT(bridge().GetSetting("name"),
              Optional(HasStringSetting("name", "value")));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridge();
  EXPECT_THAT(bridge().GetSetting("name"),
              Optional(HasStringSetting("name", "value")));
}

TEST_F(PlusAddressSettingSyncBridgeTest,
       ApplyIncrementalSyncChanges_AddUpdate) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name1", "string")}));

  // Simulate receiving an incremental add and an update to the existing entity.
  syncer::EntityChangeList change_list;
  change_list.push_back(syncer::EntityChange::CreateAdd(
      "name2", EntityFromSpecifics(CreateSettingSpecifics("name2", 123))));
  change_list.push_back(syncer::EntityChange::CreateUpdate(
      "name1",
      EntityFromSpecifics(CreateSettingSpecifics("name1", "new-string"))));
  EXPECT_FALSE(bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(change_list)));

  // Expect that the setting is available immediately.
  EXPECT_THAT(bridge().GetSetting("name1"),
              Optional(HasStringSetting("name1", "new-string")));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasIntSetting("name2", 123)));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridge();
  EXPECT_THAT(bridge().GetSetting("name1"),
              Optional(HasStringSetting("name1", "new-string")));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasIntSetting("name2", 123)));
}

TEST_F(PlusAddressSettingSyncBridgeTest, ApplyIncrementalSyncChanges_Remove) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name1", true),
                                  CreateSettingSpecifics("name2", "string")}));
  ASSERT_THAT(bridge().GetSetting("name1"),
              Optional(HasBoolSetting("name1", true)));
  ASSERT_THAT(bridge().GetSetting("name2"),
              Optional(HasStringSetting("name2", "string")));

  syncer::EntityChangeList change_list;
  change_list.push_back(syncer::EntityChange::CreateDelete("name1"));
  EXPECT_FALSE(bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(change_list)));
  // Expect that the change was applied immediately.
  EXPECT_FALSE(bridge().GetSetting("name1"));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasStringSetting("name2", "string")));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridge();
  EXPECT_FALSE(bridge().GetSetting("name1"));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasStringSetting("name2", "string")));
}

TEST_F(PlusAddressSettingSyncBridgeTest, WriteSetting) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name", false)}));
  ASSERT_THAT(bridge().GetSetting("name"),
              Optional(HasBoolSetting("name", false)));

  EXPECT_CALL(mock_processor(), Put("name", _, _));
  bridge().WriteSetting(CreateSettingSpecifics("name", true));

  EXPECT_THAT(bridge().GetSetting("name"),
              Optional(HasBoolSetting("name", true)));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridge();
  EXPECT_THAT(bridge().GetSetting("name"),
              Optional(HasBoolSetting("name", true)));
}

TEST_F(PlusAddressSettingSyncBridgeTest, ApplyDisableSyncChanges) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name", "value")}));
  ASSERT_THAT(bridge().GetSetting("name"),
              Optional(HasStringSetting("name", "value")));
  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  // Expect that the change was applied immediately.
  EXPECT_FALSE(bridge().GetSetting("name"));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridge();
  EXPECT_FALSE(bridge().GetSetting("name"));
}

TEST_F(PlusAddressSettingSyncBridgeTest, GetDataForCommit) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name1", "string"),
                                  CreateSettingSpecifics("name2", true)}));
  EXPECT_THAT(ExtractSpecificsFromBatch(bridge().GetDataForCommit({"name1"})),
              UnorderedElementsAre(HasStringSetting("name1", "string")));
}

TEST_F(PlusAddressSettingSyncBridgeTest, GetAllDataForDebugging) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name1", "string"),
                                  CreateSettingSpecifics("name2", true),
                                  CreateSettingSpecifics("name3", 123)}));
  EXPECT_THAT(ExtractSpecificsFromBatch(bridge().GetAllDataForDebugging()),
              UnorderedElementsAre(HasStringSetting("name1", "string"),
                                   HasBoolSetting("name2", true),
                                   HasIntSetting("name3", 123)));
}

}  // namespace

}  // namespace plus_addresses
