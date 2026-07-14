// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_item_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync_tab_context/container_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_tab_context {
namespace {

using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

class TabContextItemSyncBridgeTest : public ::testing::Test {
 protected:
  TabContextItemSyncBridgeTest() {
    bridge_ = std::make_unique<TabContextItemSyncBridge>(
        mock_processor_.CreateForwardingProcessor());
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<TabContextItemSyncBridge> bridge_;
};

TEST_F(TabContextItemSyncBridgeTest, ShouldUploadItemWhenTrackingMetadata) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  sync_pb::EncryptedData encrypted_data;
  encrypted_data.set_key_name("key_name");
  encrypted_data.set_blob("encrypted_blob");

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));
  EXPECT_CALL(mock_processor_, Put);

  EXPECT_TRUE(
      bridge_->UploadItem(container_id, item_id, std::move(encrypted_data)));
}

TEST_F(TabContextItemSyncBridgeTest,
       ShouldNotUploadItemWhenNotTrackingMetadata) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  sync_pb::EncryptedData encrypted_data;

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(false));
  EXPECT_CALL(mock_processor_, Put).Times(0);

  EXPECT_FALSE(
      bridge_->UploadItem(container_id, item_id, std::move(encrypted_data)));
}

TEST_F(TabContextItemSyncBridgeTest, ShouldReturnEmptyDataForCommit) {
  std::unique_ptr<syncer::DataBatch> batch =
      bridge_->GetDataForCommit({"some_key"});
  ASSERT_THAT(batch, NotNull());
  EXPECT_FALSE(batch->HasNext());
}

TEST_F(TabContextItemSyncBridgeTest, ShouldReturnEmptyDataForDebugging) {
  std::unique_ptr<syncer::DataBatch> batch = bridge_->GetAllDataForDebugging();
  ASSERT_THAT(batch, NotNull());
  EXPECT_FALSE(batch->HasNext());
}

TEST_F(TabContextItemSyncBridgeTest, ShouldComputeClientTagAndStorageKey) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item456";

  syncer::EntityData entity_data;
  sync_pb::EncryptedTabContextItemSpecifics* specifics =
      entity_data.specifics.mutable_encrypted_tab_context_item();
  specifics->set_container_id(container_id.value().AsLowercaseString());
  specifics->set_item_id(item_id);

  const std::string expected_key =
      container_id.value().AsLowercaseString() + ":" + item_id;
  EXPECT_EQ(bridge_->GetClientTag(entity_data), expected_key);
  EXPECT_EQ(bridge_->GetStorageKey(entity_data), expected_key);
}

TEST_F(TabContextItemSyncBridgeTest, ShouldValidateEntityData) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());

  syncer::EntityData valid_entity;
  sync_pb::EncryptedTabContextItemSpecifics* specifics =
      valid_entity.specifics.mutable_encrypted_tab_context_item();
  specifics->set_container_id(container_id.value().AsLowercaseString());
  specifics->set_item_id("item1");
  specifics->mutable_encrypted_data()->set_blob("blob");

  EXPECT_TRUE(bridge_->IsEntityDataValid(valid_entity));

  syncer::EntityData invalid_container_entity;
  sync_pb::EncryptedTabContextItemSpecifics* invalid_spec1 =
      invalid_container_entity.specifics.mutable_encrypted_tab_context_item();
  invalid_spec1->set_container_id("not-a-uuid");
  invalid_spec1->set_item_id("item1");
  invalid_spec1->mutable_encrypted_data()->set_blob("blob");

  EXPECT_FALSE(bridge_->IsEntityDataValid(invalid_container_entity));

  syncer::EntityData missing_item_entity;
  sync_pb::EncryptedTabContextItemSpecifics* invalid_spec2 =
      missing_item_entity.specifics.mutable_encrypted_tab_context_item();
  invalid_spec2->set_container_id(container_id.value().AsLowercaseString());
  invalid_spec2->mutable_encrypted_data()->set_blob("blob");

  EXPECT_FALSE(bridge_->IsEntityDataValid(missing_item_entity));
}

}  // namespace
}  // namespace sync_tab_context
