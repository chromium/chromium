// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_container_sync_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/encrypted_tab_context_container_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_tab_context {
namespace {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

std::unique_ptr<syncer::AgileSymmetricKeySet> CreateKeySet() {
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();
  return key_set;
}

syncer::EntityData CreateEntityDataWithEncryptionKey(
    const ContainerId& container_id,
    const syncer::AgileSymmetricKeySet& key_set) {
  syncer::EntityData entity_data;
  sync_pb::EncryptedTabContextContainerSpecifics* specifics =
      entity_data.specifics.mutable_encrypted_tab_context_container();
  specifics->set_uuid(container_id.value().AsLowercaseString());
  *specifics->mutable_encryption_key() = key_set.ToProto();
  entity_data.name = container_id.value().AsLowercaseString();
  return entity_data;
}

syncer::EntityData CreateEntityDataWithAccessToken(
    const ContainerId& container_id,
    const std::string& access_token) {
  syncer::EntityData entity_data;
  sync_pb::EncryptedTabContextContainerSpecifics* specifics =
      entity_data.specifics.mutable_encrypted_tab_context_container();
  specifics->set_uuid(container_id.value().AsLowercaseString());
  specifics->set_container_access_token(access_token);
  entity_data.name = container_id.value().AsLowercaseString();
  return entity_data;
}

void WaitUntilModelReadyToSync(
    syncer::MockDataTypeLocalChangeProcessor& mock_processor) {
  base::RunLoop run_loop;
  ON_CALL(mock_processor, ModelReadyToSync)
      .WillByDefault([&](std::unique_ptr<syncer::MetadataBatch>) {
        ON_CALL(mock_processor, IsTrackingMetadata).WillByDefault(Return(true));
        run_loop.Quit();
      });
  run_loop.Run();
}

class TabContextContainerSyncBridgeTest : public ::testing::Test {
 protected:
  TabContextContainerSyncBridgeTest() {
    bridge_ = std::make_unique<TabContextContainerSyncBridge>(
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        mock_processor_.CreateForwardingProcessor());
    WaitUntilModelReadyToSync(mock_processor_);
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<TabContextContainerSyncBridge> bridge_;
};

TEST_F(TabContextContainerSyncBridgeTest,
       ShouldCreateContainerWithoutAccessToken) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set = CreateKeySet();
  const uint32_t primary_key_id = key_set->primary_key_id();

  std::unique_ptr<syncer::EntityData> put_entity_data;
  EXPECT_CALL(mock_processor_, Put(container_id.value().AsLowercaseString(),
                                   NotNull(), NotNull()))
      .WillOnce(
          [&put_entity_data](const std::string&,
                             std::unique_ptr<syncer::EntityData> entity_data,
                             syncer::MetadataChangeList*) {
            put_entity_data = std::move(entity_data);
          });

  bridge_->CreateContainer(container_id, std::move(key_set),
                           /*pre_minted_access_token=*/std::nullopt);

  const syncer::AgileSymmetricKeySet* stored_key_set =
      bridge_->GetStoredEncryptionKeyForContainer(container_id);
  ASSERT_THAT(stored_key_set, NotNull());
  EXPECT_EQ(stored_key_set->primary_key_id(), primary_key_id);
  EXPECT_EQ(bridge_->GetStoredPreMintedAccessTokenForContainer(container_id),
            std::nullopt);

  ASSERT_THAT(put_entity_data, NotNull());
  const sync_pb::EncryptedTabContextContainerSpecifics& specifics =
      put_entity_data->specifics.encrypted_tab_context_container();
  EXPECT_EQ(specifics.uuid(), container_id.value().AsLowercaseString());
  EXPECT_TRUE(specifics.has_encryption_key());
  EXPECT_FALSE(specifics.has_container_access_token());
}

TEST_F(TabContextContainerSyncBridgeTest,
       ShouldCreateContainerWithAccessTokenAndStripKeyOnCommit) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set = CreateKeySet();
  const uint32_t primary_key_id = key_set->primary_key_id();
  const std::string kAccessToken = "test_access_token_bytes";

  std::unique_ptr<syncer::EntityData> put_entity_data;
  EXPECT_CALL(mock_processor_, Put(container_id.value().AsLowercaseString(),
                                   NotNull(), NotNull()))
      .WillOnce(
          [&put_entity_data](const std::string&,
                             std::unique_ptr<syncer::EntityData> entity_data,
                             syncer::MetadataChangeList*) {
            put_entity_data = std::move(entity_data);
          });

  bridge_->CreateContainer(container_id, std::move(key_set), kAccessToken);

  // Locally, both the encryption key and access token are available.
  const syncer::AgileSymmetricKeySet* stored_key_set =
      bridge_->GetStoredEncryptionKeyForContainer(container_id);
  ASSERT_THAT(stored_key_set, NotNull());
  EXPECT_EQ(stored_key_set->primary_key_id(), primary_key_id);
  EXPECT_THAT(bridge_->GetStoredPreMintedAccessTokenForContainer(container_id),
              Optional(Eq(kAccessToken)));

  // In the `EntityData` sent to Sync via `Put()`, `encryption_key` is stripped.
  ASSERT_THAT(put_entity_data, NotNull());
  const sync_pb::EncryptedTabContextContainerSpecifics& put_specifics =
      put_entity_data->specifics.encrypted_tab_context_container();
  EXPECT_EQ(put_specifics.uuid(), container_id.value().AsLowercaseString());
  EXPECT_FALSE(put_specifics.has_encryption_key());
  EXPECT_EQ(put_specifics.container_access_token(), kAccessToken);

  // `GetDataForCommit()` also strips `encryption_key` when `access_token` is
  // present.
  std::unique_ptr<syncer::DataBatch> batch =
      bridge_->GetDataForCommit({container_id.value().AsLowercaseString()});
  ASSERT_THAT(batch, NotNull());
  ASSERT_TRUE(batch->HasNext());
  const syncer::KeyAndData& pair = batch->Next();
  const sync_pb::EncryptedTabContextContainerSpecifics& commit_specifics =
      pair.second->specifics.encrypted_tab_context_container();
  EXPECT_EQ(commit_specifics.uuid(), container_id.value().AsLowercaseString());
  EXPECT_FALSE(commit_specifics.has_encryption_key());
  EXPECT_EQ(commit_specifics.container_access_token(), kAccessToken);
  EXPECT_FALSE(batch->HasNext());
}

TEST_F(TabContextContainerSyncBridgeTest,
       ShouldStoreKeySetOnRemoteAddAndUpdate) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set = CreateKeySet();

  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateAdd(
      container_id.value().AsLowercaseString(),
      CreateEntityDataWithEncryptionKey(container_id, *key_set)));

  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(changes));

  const syncer::AgileSymmetricKeySet* stored_key_set =
      bridge_->GetStoredEncryptionKeyForContainer(container_id);
  ASSERT_THAT(stored_key_set, NotNull());
  EXPECT_EQ(stored_key_set->primary_key_id(), key_set->primary_key_id());
  EXPECT_EQ(bridge_->GetStoredPreMintedAccessTokenForContainer(container_id),
            std::nullopt);
}

TEST_F(TabContextContainerSyncBridgeTest,
       ShouldStoreAccessTokenWithoutEncryptionKeyOnRemoteAdd) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string kAccessToken = "remote_access_token_bytes";

  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateAdd(
      container_id.value().AsLowercaseString(),
      CreateEntityDataWithAccessToken(container_id, kAccessToken)));

  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(changes));

  EXPECT_THAT(bridge_->GetStoredEncryptionKeyForContainer(container_id),
              IsNull());
  EXPECT_THAT(bridge_->GetStoredPreMintedAccessTokenForContainer(container_id),
              Optional(Eq(kAccessToken)));
}

TEST_F(TabContextContainerSyncBridgeTest,
       ShouldPreserveLocalEncryptionKeyWhenRemoteUpdateLacksEncryptionKey) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set = CreateKeySet();
  const uint32_t primary_key_id = key_set->primary_key_id();
  const std::string kInitialAccessToken = "initial_access_token";
  const std::string kUpdatedAccessToken = "updated_access_token";

  bridge_->CreateContainer(container_id, std::move(key_set),
                           kInitialAccessToken);

  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateUpdate(
      container_id.value().AsLowercaseString(),
      CreateEntityDataWithAccessToken(container_id, kUpdatedAccessToken)));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(changes));

  const syncer::AgileSymmetricKeySet* stored_key_set =
      bridge_->GetStoredEncryptionKeyForContainer(container_id);
  ASSERT_THAT(stored_key_set, NotNull());
  EXPECT_EQ(stored_key_set->primary_key_id(), primary_key_id);
  EXPECT_THAT(bridge_->GetStoredPreMintedAccessTokenForContainer(container_id),
              Optional(Eq(kUpdatedAccessToken)));
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldRemoveKeySetOnRemoteDelete) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set = CreateKeySet();

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      container_id.value().AsLowercaseString(),
      CreateEntityDataWithEncryptionKey(container_id, *key_set)));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(add_changes));
  ASSERT_THAT(bridge_->GetStoredEncryptionKeyForContainer(container_id),
              NotNull());

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete(
      container_id.value().AsLowercaseString(), syncer::EntityData()));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(delete_changes));
  EXPECT_THAT(bridge_->GetStoredEncryptionKeyForContainer(container_id),
              IsNull());
  EXPECT_EQ(bridge_->GetStoredPreMintedAccessTokenForContainer(container_id),
            std::nullopt);
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldReturnDataForCommit) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  bridge_->CreateContainer(container_id, CreateKeySet(),
                           /*pre_minted_access_token=*/std::nullopt);

  std::unique_ptr<syncer::DataBatch> batch =
      bridge_->GetDataForCommit({container_id.value().AsLowercaseString()});
  ASSERT_THAT(batch, NotNull());
  EXPECT_TRUE(batch->HasNext());
  const syncer::KeyAndData& pair = batch->Next();
  EXPECT_EQ(pair.first, container_id.value().AsLowercaseString());
  const sync_pb::EncryptedTabContextContainerSpecifics& specifics =
      pair.second->specifics.encrypted_tab_context_container();
  EXPECT_EQ(specifics.uuid(), container_id.value().AsLowercaseString());
  EXPECT_TRUE(specifics.has_encryption_key());
  EXPECT_FALSE(specifics.has_container_access_token());
  EXPECT_FALSE(batch->HasNext());
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldReturnAllDataForDebugging) {
  const ContainerId container1(base::Uuid::GenerateRandomV4());
  const ContainerId container2(base::Uuid::GenerateRandomV4());
  bridge_->CreateContainer(container1, CreateKeySet(),
                           /*pre_minted_access_token=*/std::nullopt);
  bridge_->CreateContainer(container2, CreateKeySet(), "token_2");

  std::unique_ptr<syncer::DataBatch> batch = bridge_->GetAllDataForDebugging();
  ASSERT_THAT(batch, NotNull());

  std::vector<std::string> keys;
  while (batch->HasNext()) {
    keys.push_back(batch->Next().first);
  }
  EXPECT_EQ(keys.size(), 2u);
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldReloadLocalDataFromStore) {
  std::unique_ptr<syncer::DataTypeStore> store =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor1;
  auto bridge1 = std::make_unique<TabContextContainerSyncBridge>(
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store.get()),
      processor1.CreateForwardingProcessor());
  WaitUntilModelReadyToSync(processor1);

  const ContainerId container1(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set1 = CreateKeySet();
  const uint32_t primary_key_id1 = key_set1->primary_key_id();
  bridge1->CreateContainer(container1, std::move(key_set1),
                           /*pre_minted_access_token=*/std::nullopt);

  const ContainerId container2(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set2 = CreateKeySet();
  const uint32_t primary_key_id2 = key_set2->primary_key_id();
  const std::string kAccessToken2 = "persisted_access_token";
  bridge1->CreateContainer(container2, std::move(key_set2), kAccessToken2);

  bridge1.reset();

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor2;
  auto bridge2 = std::make_unique<TabContextContainerSyncBridge>(
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store.get()),
      processor2.CreateForwardingProcessor());
  WaitUntilModelReadyToSync(processor2);

  const syncer::AgileSymmetricKeySet* reloaded_key_set1 =
      bridge2->GetStoredEncryptionKeyForContainer(container1);
  ASSERT_THAT(reloaded_key_set1, NotNull());
  EXPECT_EQ(reloaded_key_set1->primary_key_id(), primary_key_id1);
  EXPECT_EQ(bridge2->GetStoredPreMintedAccessTokenForContainer(container1),
            std::nullopt);

  // Even for ephemeral containers with an access token, the local store
  // preserves the encryption key across restarts on the originating device.
  const syncer::AgileSymmetricKeySet* reloaded_key_set2 =
      bridge2->GetStoredEncryptionKeyForContainer(container2);
  ASSERT_THAT(reloaded_key_set2, NotNull());
  EXPECT_EQ(reloaded_key_set2->primary_key_id(), primary_key_id2);
  EXPECT_THAT(bridge2->GetStoredPreMintedAccessTokenForContainer(container2),
              Optional(Eq(kAccessToken2)));
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldValidateEntityData) {
  const ContainerId valid_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> valid_key = CreateKeySet();

  EXPECT_TRUE(bridge_->IsEntityDataValid(
      CreateEntityDataWithEncryptionKey(valid_id, *valid_key)));
  EXPECT_TRUE(bridge_->IsEntityDataValid(
      CreateEntityDataWithAccessToken(valid_id, "valid_token")));
  EXPECT_FALSE(bridge_->IsEntityDataValid(
      CreateEntityDataWithAccessToken(valid_id, /*access_token=*/"")));

  syncer::EntityData empty_entity_data;
  empty_entity_data.specifics.mutable_encrypted_tab_context_container()
      ->set_uuid(valid_id.value().AsLowercaseString());
  EXPECT_FALSE(bridge_->IsEntityDataValid(empty_entity_data));
}

}  // namespace
}  // namespace sync_tab_context
