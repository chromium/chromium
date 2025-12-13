// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_util.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/account_setting_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using SettingSpecifics = sync_pb::AccountSettingSpecifics;
using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::UnorderedPointwise;

syncer::EntityData EntityFromSpecifics(const SettingSpecifics& specifics) {
  syncer::EntityData entity;
  *entity.specifics.mutable_account_setting() = specifics;
  return entity;
}

// Assumes that `batch` only contains entities with `SettingSpecifics` and
// extracts them into a vector.
std::vector<SettingSpecifics> ExtractSpecificsFromBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<SettingSpecifics> specifics;
  while (batch->HasNext()) {
    std::unique_ptr<syncer::EntityData> entity = batch->Next().second;
    CHECK(entity->specifics.has_account_setting());
    specifics.push_back(entity->specifics.account_setting());
  }
  return specifics;
}

class MockObserver : public AccountSettingSyncBridge::Observer {
 public:
  explicit MockObserver(AccountSettingSyncBridge* bridge) {
    scoped_observation_.Observe(bridge);
  }

  MOCK_METHOD(void, OnDataLoadedFromDisk, (), (override));

 private:
  base::ScopedObservation<AccountSettingSyncBridge,
                          AccountSettingSyncBridge::Observer>
      scoped_observation_{this};
};

class AccountSettingSyncBridgeTest : public testing::Test {
 public:
  AccountSettingSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    RecreateBridgeAndWaitForModelToSync();
  }

  void RecreateBridge() {
    bridge_ = std::make_unique<AccountSettingSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
  }

  template <typename... Args>
  void RecreateBridgeAndWaitForModelToSync(Args&&... args) {
    // Even though the test uses an in-memory store, it still posts tasks. Wait
    // for initialisation to complete.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_processor(), ModelReadyToSync(std::forward<Args>(args)...))
        .WillRepeatedly(base::test::RunClosure(run_loop.QuitClosure()));

    RecreateBridge();
    run_loop.Run();
  }
  void RecreateBridgeAndWaitForModelToSync() {
    RecreateBridgeAndWaitForModelToSync(_);
  }

  AccountSettingSyncBridge& bridge() { return *bridge_; }

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
    return !bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<AccountSettingSyncBridge> bridge_;
};

TEST_F(AccountSettingSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity;
  *entity.specifics.mutable_account_setting() =
      CreateSettingSpecifics("name", "value");
  EXPECT_EQ(bridge().GetStorageKey(entity), "name");
  // `GetClientTag()` is implemented using `GetStorageKey()`.
  EXPECT_EQ(bridge().GetClientTag(entity), "name");
}

TEST_F(AccountSettingSyncBridgeTest, ModelReadyToSync_InitialSync) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_processor(), ModelReadyToSync)
      .WillRepeatedly(base::test::RunClosure(run_loop.QuitClosure()));

  RecreateBridge();

  MockObserver o(&bridge());
  EXPECT_CALL(o, OnDataLoadedFromDisk);

  run_loop.Run();
}

TEST_F(AccountSettingSyncBridgeTest, ModelReadyToSync_ExistingMetadata) {
  // Simulate that some metadata is stored.
  sync_pb::DataTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::DataTypeState::INITIAL_SYNC_DONE);
  auto write_batch = store().CreateWriteBatch();
  write_batch->GetMetadataChangeList()->UpdateDataTypeState(model_type_state);
  base::test::TestFuture<const std::optional<syncer::ModelError>&> write_result;
  store().CommitWriteBatch(std::move(write_batch), write_result.GetCallback());
  ASSERT_FALSE(write_result.Get());

  // Expect that `ModelReadyToSync()` is called with the stored metadata when
  // the bridge is created.
  RecreateBridgeAndWaitForModelToSync(syncer::MetadataBatchContains(
      /*state=*/syncer::HasInitialSyncDone(),
      /*entities=*/testing::IsEmpty()));
}

TEST_F(AccountSettingSyncBridgeTest, MergeFullSyncData) {
  EXPECT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name", "value")}));
  // Expect that the setting is available immediately.
  EXPECT_THAT(bridge().GetStringSetting("name"), "value");
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridgeAndWaitForModelToSync();
  EXPECT_THAT(bridge().GetStringSetting("name"), "value");
}

TEST_F(AccountSettingSyncBridgeTest, ApplyIncrementalSyncChanges_AddUpdate) {
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
  EXPECT_THAT(bridge().GetStringSetting("name1"), "new-string");
  EXPECT_THAT(bridge().GetIntSetting("name2"), 123);
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridgeAndWaitForModelToSync();
  EXPECT_THAT(bridge().GetStringSetting("name1"), "new-string");
  EXPECT_THAT(bridge().GetIntSetting("name2"), 123);
}

TEST_F(AccountSettingSyncBridgeTest, ApplyIncrementalSyncChanges_Remove) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name1", true),
                                  CreateSettingSpecifics("name2", "string")}));
  ASSERT_THAT(bridge().GetBoolSetting("name1"), true);
  ASSERT_THAT(bridge().GetStringSetting("name2"), "string");

  syncer::EntityChangeList change_list;
  change_list.push_back(
      syncer::EntityChange::CreateDelete("name1", syncer::EntityData()));
  EXPECT_FALSE(bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(change_list)));
  // Expect that the change was applied immediately.
  EXPECT_FALSE(bridge().GetBoolSetting("name1").has_value());
  EXPECT_THAT(bridge().GetStringSetting("name2"), "string");
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridgeAndWaitForModelToSync();
  EXPECT_FALSE(bridge().GetBoolSetting("name1").has_value());
  EXPECT_THAT(bridge().GetStringSetting("name2"), "string");
}

TEST_F(AccountSettingSyncBridgeTest, ApplyDisableSyncChanges) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name", "value")}));
  ASSERT_THAT(bridge().GetStringSetting("name"), "value");
  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  // Expect that the change was applied immediately.
  EXPECT_FALSE(bridge().GetStringSetting("name").has_value());
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridgeAndWaitForModelToSync();
  EXPECT_FALSE(bridge().GetStringSetting("name").has_value());
}

TEST_F(AccountSettingSyncBridgeTest, GetAllDataForDebugging) {
  sync_pb::AccountSettingSpecifics bool_setting =
      CreateSettingSpecifics("name1", true);
  sync_pb::AccountSettingSpecifics int_setting =
      CreateSettingSpecifics("name2", 123);
  sync_pb::AccountSettingSpecifics string_setting =
      CreateSettingSpecifics("name3", "string");
  ASSERT_TRUE(
      StartSyncingWithServerData({bool_setting, int_setting, string_setting}));
  EXPECT_THAT(ExtractSpecificsFromBatch(bridge().GetAllDataForDebugging()),
              UnorderedPointwise(EqualsProto(),
                                 {bool_setting, int_setting, string_setting}));
}

TEST_F(AccountSettingSyncBridgeTest, IsEntityDataValid) {
  SettingSpecifics specifics = CreateSettingSpecifics("name", "value");
  EXPECT_TRUE(bridge().IsEntityDataValid(EntityFromSpecifics(specifics)));

  // Specifics with an empty name are invalid.
  specifics.mutable_name()->clear();
  EXPECT_FALSE(bridge().IsEntityDataValid(EntityFromSpecifics(specifics)));

  // Specifics with a missing name are invalid.
  specifics.clear_name();
  EXPECT_FALSE(bridge().IsEntityDataValid(EntityFromSpecifics(specifics)));
}

}  // namespace

}  // namespace autofill
