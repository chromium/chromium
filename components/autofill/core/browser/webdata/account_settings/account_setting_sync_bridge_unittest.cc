// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_test_util.h"
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
using ::testing::_;
using ::testing::Optional;

syncer::EntityData EntityFromSpecifics(const SettingSpecifics& specifics) {
  syncer::EntityData entity;
  *entity.specifics.mutable_account_setting() = specifics;
  return entity;
}

class AccountSettingSyncBridgeTest : public testing::Test {
 public:
  AccountSettingSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    RecreateBridgeAnsWaitForModelToSync();
  }

  template <typename... Args>
  void RecreateBridgeAnsWaitForModelToSync(Args&&... args) {
    // Even though the test uses an in-memory store, it still posts tasks. Wait
    // for initialisation to complete.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_processor(), ModelReadyToSync(std::forward<Args>(args)...))
        .WillRepeatedly(base::test::RunClosure(run_loop.QuitClosure()));

    bridge_ = std::make_unique<AccountSettingSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));

    run_loop.Run();
  }
  void RecreateBridgeAnsWaitForModelToSync() {
    RecreateBridgeAnsWaitForModelToSync(_);
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
  RecreateBridgeAnsWaitForModelToSync();
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
  RecreateBridgeAnsWaitForModelToSync(syncer::MetadataBatchContains(
      /*state=*/syncer::HasInitialSyncDone(),
      /*entities=*/testing::IsEmpty()));
}

TEST_F(AccountSettingSyncBridgeTest, MergeFullSyncData) {
  EXPECT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name", "value")}));
  // Expect that the setting is available immediately.
  EXPECT_THAT(bridge().GetSetting("name"),
              Optional(HasStringSetting("name", "value")));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridgeAnsWaitForModelToSync();
  EXPECT_THAT(bridge().GetSetting("name"),
              Optional(HasStringSetting("name", "value")));
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
  EXPECT_THAT(bridge().GetSetting("name1"),
              Optional(HasStringSetting("name1", "new-string")));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasIntSetting("name2", 123)));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridgeAnsWaitForModelToSync();
  EXPECT_THAT(bridge().GetSetting("name1"),
              Optional(HasStringSetting("name1", "new-string")));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasIntSetting("name2", 123)));
}

TEST_F(AccountSettingSyncBridgeTest, ApplyIncrementalSyncChanges_Remove) {
  ASSERT_TRUE(
      StartSyncingWithServerData({CreateSettingSpecifics("name", true),
                                  CreateSettingSpecifics("name2", "string")}));

  syncer::EntityChangeList change_list;
  change_list.push_back(
      syncer::EntityChange::CreateDelete("name1", syncer::EntityData()));
  EXPECT_FALSE(bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(change_list)));
  // Expect that the change was applied immediately.
  EXPECT_FALSE(bridge().GetSetting("name1"));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasStringSetting("name2", "string")));
  // Recreate the bridge, reloading from the `store()`.
  RecreateBridgeAnsWaitForModelToSync();
  EXPECT_FALSE(bridge().GetSetting("name1"));
  EXPECT_THAT(bridge().GetSetting("name2"),
              Optional(HasStringSetting("name2", "string")));
}

}  // namespace

}  // namespace autofill
