// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebook_sync_bridge.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/notebook_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

namespace {

constexpr char kTestUuid[] = "12345678-1234-1234-1234-123456789012";

sync_pb::NotebookSpecifics CreateTestNotebookSpecifics(
    const std::string& uuid) {
  sync_pb::NotebookSpecifics specifics;
  specifics.set_uuid(uuid);
  specifics.set_creation_time_windows_epoch_micros(100);
  specifics.set_update_time_windows_epoch_micros(200);
  specifics.mutable_notebook();
  specifics.set_schema_version(1);
  return specifics;
}

syncer::EntityData CreateTestEntityData(const std::string& uuid) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_notebook() = CreateTestNotebookSpecifics(uuid);
  entity_data.name = uuid;
  return entity_data;
}

class NotebookSyncBridgeTest : public testing::Test {
 public:
  NotebookSyncBridgeTest() = default;
  ~NotebookSyncBridgeTest() override = default;

  void SetUp() override {
    bridge_ = std::make_unique<NotebookSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
  }

  NotebookSyncBridge& bridge() { return *bridge_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<NotebookSyncBridge> bridge_;
};

TEST_F(NotebookSyncBridgeTest, GetClientTag) {
  syncer::EntityData data = CreateTestEntityData(kTestUuid);
  EXPECT_EQ(bridge().GetClientTag(data), kTestUuid);
}

TEST_F(NotebookSyncBridgeTest, GetStorageKey) {
  syncer::EntityData data = CreateTestEntityData(kTestUuid);
  EXPECT_EQ(bridge().GetStorageKey(data), kTestUuid);
}

TEST_F(NotebookSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData data = CreateTestEntityData(kTestUuid);
  EXPECT_TRUE(bridge().IsEntityDataValid(data));

  syncer::EntityData invalid_data = CreateTestEntityData("");
  EXPECT_FALSE(bridge().IsEntityDataValid(invalid_data));
}

TEST_F(NotebookSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_notebook() = CreateTestNotebookSpecifics(kTestUuid);

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(specifics);

  EXPECT_FALSE(trimmed_specifics.notebook().has_uuid());
  EXPECT_FALSE(
      trimmed_specifics.notebook().has_creation_time_windows_epoch_micros());
  EXPECT_FALSE(
      trimmed_specifics.notebook().has_update_time_windows_epoch_micros());
  EXPECT_FALSE(trimmed_specifics.notebook().has_notebook());
  EXPECT_FALSE(trimmed_specifics.notebook().has_schema_version());
}

TEST_F(NotebookSyncBridgeTest, ApplyIncrementalSyncChanges) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  std::optional<syncer::ModelError> error =
      bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                           std::move(add_changes));
  EXPECT_FALSE(error);
  EXPECT_EQ(bridge().entries_for_testing().size(), 1u);

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete(kTestUuid, syncer::EntityData()));
  error = bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(delete_changes));
  EXPECT_FALSE(error);
  EXPECT_EQ(bridge().entries_for_testing().size(), 0u);
}

TEST_F(NotebookSyncBridgeTest, GetDataForCommit) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  std::unique_ptr<syncer::DataBatch> batch =
      bridge().GetDataForCommit({kTestUuid});
  ASSERT_TRUE(batch);
  EXPECT_TRUE(batch->HasNext());
  auto [key, data] = batch->Next();
  EXPECT_EQ(key, kTestUuid);
  EXPECT_EQ(data->specifics.notebook().uuid(), kTestUuid);
  EXPECT_FALSE(batch->HasNext());
}

TEST_F(NotebookSyncBridgeTest, GetAllDataForDebugging) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  std::unique_ptr<syncer::DataBatch> batch = bridge().GetAllDataForDebugging();
  ASSERT_TRUE(batch);
  EXPECT_TRUE(batch->HasNext());
  auto [key, data] = batch->Next();
  EXPECT_EQ(key, kTestUuid);
  EXPECT_EQ(data->specifics.notebook().uuid(), kTestUuid);
  EXPECT_FALSE(batch->HasNext());
}

TEST_F(NotebookSyncBridgeTest, ApplyDisableSyncChanges) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  EXPECT_EQ(bridge().entries_for_testing().size(), 1u);

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  EXPECT_EQ(bridge().entries_for_testing().size(), 0u);
}

}  // namespace

}  // namespace notebooks
