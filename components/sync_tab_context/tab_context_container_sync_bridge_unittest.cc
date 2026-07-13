// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_container_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_tab_context {
namespace {

using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

syncer::EntityData CreateEntityData(
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

class TabContextContainerSyncBridgeTest : public ::testing::Test {
 protected:
  TabContextContainerSyncBridgeTest() {
    bridge_ = std::make_unique<TabContextContainerSyncBridge>(
        mock_processor_.CreateForwardingProcessor());
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<TabContextContainerSyncBridge> bridge_;
};

TEST_F(TabContextContainerSyncBridgeTest, ShouldCreateContainerAndGenerateKey) {
  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));
  EXPECT_CALL(mock_processor_, Put);

  std::optional<ContainerId> container_id = bridge_->CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  const syncer::AgileSymmetricKeySet* key_set =
      bridge_->GetEncryptionKeyForContainer(*container_id);
  ASSERT_THAT(key_set, NotNull());
  EXPECT_GT(key_set->size(), 0u);
  EXPECT_NE(key_set->primary_key_id(), 0u);
}

TEST_F(TabContextContainerSyncBridgeTest,
       ShouldNotCreateContainerWhenNotTrackingMetadata) {
  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(false));
  EXPECT_CALL(mock_processor_, Put).Times(0);

  std::optional<ContainerId> container_id = bridge_->CreateContainer();
  EXPECT_FALSE(container_id.has_value());
}

TEST_F(TabContextContainerSyncBridgeTest,
       ShouldStoreKeySetOnRemoteAddAndUpdate) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();

  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateAdd(
      container_id.value().AsLowercaseString(),
      CreateEntityData(container_id, *key_set)));

  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(changes));

  const syncer::AgileSymmetricKeySet* stored_key_set =
      bridge_->GetEncryptionKeyForContainer(container_id);
  ASSERT_THAT(stored_key_set, NotNull());
  EXPECT_EQ(stored_key_set->primary_key_id(), key_set->primary_key_id());
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldRemoveKeySetOnRemoteDelete) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      container_id.value().AsLowercaseString(),
      CreateEntityData(container_id, *key_set)));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(add_changes));
  ASSERT_THAT(bridge_->GetEncryptionKeyForContainer(container_id), NotNull());

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete(
      container_id.value().AsLowercaseString(), syncer::EntityData()));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(delete_changes));
  EXPECT_THAT(bridge_->GetEncryptionKeyForContainer(container_id), IsNull());
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldReturnDataForCommit) {
  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));
  std::optional<ContainerId> container_id = bridge_->CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  std::unique_ptr<syncer::DataBatch> batch =
      bridge_->GetDataForCommit({container_id->value().AsLowercaseString()});
  ASSERT_THAT(batch, NotNull());
  EXPECT_TRUE(batch->HasNext());
  const syncer::KeyAndData& pair = batch->Next();
  EXPECT_EQ(pair.first, container_id->value().AsLowercaseString());
  EXPECT_EQ(pair.second->specifics.encrypted_tab_context_container().uuid(),
            container_id->value().AsLowercaseString());
  EXPECT_FALSE(batch->HasNext());
}

TEST_F(TabContextContainerSyncBridgeTest, ShouldReturnAllDataForDebugging) {
  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));
  std::optional<ContainerId> container1 = bridge_->CreateContainer();
  std::optional<ContainerId> container2 = bridge_->CreateContainer();
  ASSERT_TRUE(container1.has_value());
  ASSERT_TRUE(container2.has_value());

  std::unique_ptr<syncer::DataBatch> batch = bridge_->GetAllDataForDebugging();
  ASSERT_THAT(batch, NotNull());

  std::vector<std::string> keys;
  while (batch->HasNext()) {
    keys.push_back(batch->Next().first);
  }
  EXPECT_EQ(keys.size(), 2u);
}

}  // namespace
}  // namespace sync_tab_context
