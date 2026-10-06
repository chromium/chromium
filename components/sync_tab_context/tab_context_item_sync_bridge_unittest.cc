// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_item_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/upload_outcome.h"
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

TEST_F(TabContextItemSyncBridgeTest,
       ShouldUploadItemAndRunCallbackOnCommitSuccess) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  sync_pb::EncryptedData encrypted_data;
  encrypted_data.set_key_name("key_name");
  encrypted_data.set_blob("encrypted_blob");

  std::string captured_storage_key;
  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));
  EXPECT_CALL(mock_processor_, Put)
      .WillOnce([&](const std::string& storage_key,
                    std::unique_ptr<syncer::EntityData> entity_data,
                    syncer::MetadataChangeList* metadata_change_list) {
        ASSERT_THAT(metadata_change_list, NotNull());
        captured_storage_key = storage_key;
        metadata_change_list->UpdateMetadata(storage_key,
                                             sync_pb::EntityMetadata());
      });

  base::test::TestFuture<UploadOutcome> upload_future;
  bridge_->UploadItem(container_id, item_id, std::move(encrypted_data),
                      upload_future.GetCallback());
  EXPECT_FALSE(upload_future.IsReady());

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(syncer::EntityChange::CreateDelete(
      captured_storage_key, syncer::EntityData()));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(entity_changes));
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kSucceeded);
}

TEST_F(TabContextItemSyncBridgeTest,
       ShouldNotUploadItemWhenNotTrackingMetadata) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  sync_pb::EncryptedData encrypted_data;

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(false));
  EXPECT_CALL(mock_processor_, Put).Times(0);

  base::test::TestFuture<UploadOutcome> upload_future;
  bridge_->UploadItem(container_id, item_id, std::move(encrypted_data),
                      upload_future.GetCallback());
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kFailed);
}

TEST_F(TabContextItemSyncBridgeTest,
       ShouldRetryOnTransientCommitErrorAndFailOnNonTransientCommitError) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  const std::string storage_key =
      container_id.value().AsLowercaseString() + ":" + item_id;
  const syncer::ClientTagHash client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::ENCRYPTED_TAB_CONTEXT_ITEM,
                                          storage_key);

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));

  base::test::TestFuture<UploadOutcome> upload_future;
  bridge_->UploadItem(container_id, item_id, sync_pb::EncryptedData(),
                      upload_future.GetCallback());
  EXPECT_FALSE(upload_future.IsReady());

  // `TRANSIENT_ERROR` should remain tracked for retry on the next sync cycle.
  EXPECT_CALL(mock_processor_, UntrackEntityForClientTagHash).Times(0);
  syncer::FailedCommitResponseData transient_error_response;
  transient_error_response.client_tag_hash = client_tag_hash;
  transient_error_response.response_type =
      sync_pb::CommitResponse::TRANSIENT_ERROR;
  bridge_->OnCommitAttemptErrors({transient_error_response});
  EXPECT_FALSE(upload_future.IsReady());

  // Non-transient per-item errors should untrack the entity and fail the
  // callback.
  EXPECT_CALL(mock_processor_, UntrackEntityForClientTagHash(client_tag_hash));
  syncer::FailedCommitResponseData permanent_error_response;
  permanent_error_response.client_tag_hash = client_tag_hash;
  permanent_error_response.response_type =
      sync_pb::CommitResponse::INVALID_MESSAGE;
  bridge_->OnCommitAttemptErrors({permanent_error_response});
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kFailed);
}

TEST_F(TabContextItemSyncBridgeTest,
       ShouldRetryOnTransientCommitAttemptFailedAndFailOnServerError) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  const std::string storage_key =
      container_id.value().AsLowercaseString() + ":" + item_id;
  const syncer::ClientTagHash client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::ENCRYPTED_TAB_CONTEXT_ITEM,
                                          storage_key);

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));

  base::test::TestFuture<UploadOutcome> upload_future;
  bridge_->UploadItem(container_id, item_id, sync_pb::EncryptedData(),
                      upload_future.GetCallback());
  EXPECT_FALSE(upload_future.IsReady());

  EXPECT_CALL(mock_processor_, UntrackEntityForClientTagHash).Times(0);
  EXPECT_EQ(
      bridge_->OnCommitAttemptFailed(syncer::SyncCommitError::kNetworkError),
      syncer::DataTypeSyncBridge::CommitAttemptFailedBehavior::
          kShouldRetryOnNextCycle);
  EXPECT_EQ(bridge_->OnCommitAttemptFailed(syncer::SyncCommitError::kAuthError),
            syncer::DataTypeSyncBridge::CommitAttemptFailedBehavior::
                kShouldRetryOnNextCycle);
  EXPECT_FALSE(upload_future.IsReady());

  EXPECT_CALL(mock_processor_, UntrackEntityForClientTagHash(client_tag_hash));
  EXPECT_EQ(
      bridge_->OnCommitAttemptFailed(syncer::SyncCommitError::kServerError),
      syncer::DataTypeSyncBridge::CommitAttemptFailedBehavior::
          kDontRetryOnNextCycle);
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kFailed);
}

TEST_F(TabContextItemSyncBridgeTest,
       ShouldFailPendingCallbacksOnApplyDisableSyncChanges) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));

  base::test::TestFuture<UploadOutcome> upload_future;
  bridge_->UploadItem(container_id, item_id, sync_pb::EncryptedData(),
                      upload_future.GetCallback());
  EXPECT_FALSE(upload_future.IsReady());

  bridge_->ApplyDisableSyncChanges(bridge_->CreateMetadataChangeList());
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kFailed);
}

TEST_F(TabContextItemSyncBridgeTest, ShouldReturnInFlightDataForCommit) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  const std::string storage_key =
      container_id.value().AsLowercaseString() + ":" + item_id;
  sync_pb::EncryptedData encrypted_data;
  encrypted_data.set_key_name("key_name");
  encrypted_data.set_blob("encrypted_blob");

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));

  base::test::TestFuture<UploadOutcome> upload_future;
  bridge_->UploadItem(container_id, item_id, std::move(encrypted_data),
                      upload_future.GetCallback());

  std::unique_ptr<syncer::DataBatch> commit_batch =
      bridge_->GetDataForCommit({storage_key, "unknown_key"});
  ASSERT_THAT(commit_batch, NotNull());
  ASSERT_TRUE(commit_batch->HasNext());
  syncer::KeyAndData key_and_data = commit_batch->Next();
  EXPECT_EQ(key_and_data.first, storage_key);
  EXPECT_EQ(key_and_data.second->specifics.encrypted_tab_context_item()
                .encrypted_content()
                .blob(),
            "encrypted_blob");
  EXPECT_FALSE(commit_batch->HasNext());

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(
      syncer::EntityChange::CreateDelete(storage_key, syncer::EntityData()));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(entity_changes));
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kSucceeded);

  EXPECT_FALSE(bridge_->GetDataForCommit({storage_key})->HasNext());
}

TEST_F(TabContextItemSyncBridgeTest, ShouldReturnInFlightDataForDebugging) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  const std::string item_id = "item123";
  const std::string storage_key =
      container_id.value().AsLowercaseString() + ":" + item_id;
  sync_pb::EncryptedData encrypted_data;
  encrypted_data.set_key_name("key_name");
  encrypted_data.set_blob("encrypted_blob");

  ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));

  base::test::TestFuture<UploadOutcome> upload_future;
  bridge_->UploadItem(container_id, item_id, std::move(encrypted_data),
                      upload_future.GetCallback());

  std::unique_ptr<syncer::DataBatch> debug_batch =
      bridge_->GetAllDataForDebugging();
  ASSERT_THAT(debug_batch, NotNull());
  ASSERT_TRUE(debug_batch->HasNext());
  syncer::KeyAndData key_and_data = debug_batch->Next();
  EXPECT_EQ(key_and_data.first, storage_key);
  EXPECT_EQ(key_and_data.second->specifics.encrypted_tab_context_item()
                .encrypted_content()
                .blob(),
            "encrypted_blob");
  EXPECT_FALSE(debug_batch->HasNext());

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(
      syncer::EntityChange::CreateDelete(storage_key, syncer::EntityData()));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(entity_changes));
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kSucceeded);

  EXPECT_FALSE(bridge_->GetAllDataForDebugging()->HasNext());
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
  specifics->mutable_encrypted_content()->set_blob("blob");

  EXPECT_TRUE(bridge_->IsEntityDataValid(valid_entity));

  syncer::EntityData invalid_container_entity;
  sync_pb::EncryptedTabContextItemSpecifics* invalid_spec1 =
      invalid_container_entity.specifics.mutable_encrypted_tab_context_item();
  invalid_spec1->set_container_id("not-a-uuid");
  invalid_spec1->set_item_id("item1");
  invalid_spec1->mutable_encrypted_content()->set_blob("blob");

  EXPECT_FALSE(bridge_->IsEntityDataValid(invalid_container_entity));

  syncer::EntityData missing_item_entity;
  sync_pb::EncryptedTabContextItemSpecifics* invalid_spec2 =
      missing_item_entity.specifics.mutable_encrypted_tab_context_item();
  invalid_spec2->set_container_id(container_id.value().AsLowercaseString());
  invalid_spec2->mutable_encrypted_content()->set_blob("blob");

  EXPECT_FALSE(bridge_->IsEntityDataValid(missing_item_entity));
}

}  // namespace
}  // namespace sync_tab_context
