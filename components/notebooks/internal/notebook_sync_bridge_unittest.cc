// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebook_sync_bridge.h"

#include <memory>
#include <utility>

#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/notebooks/internal/notebooks_model.h"
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
        &model_, mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
  }

  NotebookSyncBridge& bridge() { return *bridge_; }
  NotebooksModel& model() { return model_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  NotebooksModel model_;
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

TEST_F(NotebookSyncBridgeTest, IsEntityDataValidReturnsTrueForValidData) {
  syncer::EntityData data = CreateTestEntityData(kTestUuid);
  EXPECT_TRUE(bridge().IsEntityDataValid(data));
}

TEST_F(NotebookSyncBridgeTest, IsEntityDataValidReturnsFalseForEmptyUuid) {
  syncer::EntityData invalid_data = CreateTestEntityData("");
  EXPECT_FALSE(bridge().IsEntityDataValid(invalid_data));
}

TEST_F(NotebookSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_notebook() = CreateTestNotebookSpecifics(kTestUuid);

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(specifics);

  EXPECT_THAT(trimmed_specifics,
              base::test::EqualsProto(sync_pb::EntitySpecifics()));
}

TEST_F(NotebookSyncBridgeTest, ApplyIncrementalSyncChangesReturnsNoError) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  std::optional<syncer::ModelError> error =
      bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                           std::move(add_changes));
  EXPECT_FALSE(error);
}

TEST_F(NotebookSyncBridgeTest, ApplyIncrementalSyncChangesAddsToBridge) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  EXPECT_THAT(bridge().entries_for_testing(),
              testing::ElementsAre(testing::Pair(
                  kTestUuid, base::test::EqualsProto(
                                 CreateTestNotebookSpecifics(kTestUuid)))));
}

TEST_F(NotebookSyncBridgeTest, ApplyIncrementalSyncChangesAddsToModel) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  EXPECT_EQ(model().GetAllNotebooks().size(), 1u);
  EXPECT_THAT(
      model().GetAllNotebooks(),
      testing::ElementsAre(Notebook(
          NotebookId(base::Uuid::ParseCaseInsensitive(kTestUuid)),
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(100)),
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(200)))));
}

TEST_F(NotebookSyncBridgeTest,
       ApplyIncrementalSyncChangesSetsCreationTimeInModel) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  std::optional<Notebook> notebook = model().GetNotebook(
      NotebookId(base::Uuid::ParseCaseInsensitive(kTestUuid)));
  ASSERT_TRUE(notebook.has_value());
  EXPECT_EQ(notebook->creation_time(),
            base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(100)));
}

TEST_F(NotebookSyncBridgeTest,
       ApplyIncrementalSyncChangesSetsUpdateTimeInModel) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  std::optional<Notebook> notebook = model().GetNotebook(
      NotebookId(base::Uuid::ParseCaseInsensitive(kTestUuid)));
  ASSERT_TRUE(notebook.has_value());
  EXPECT_EQ(notebook->update_time(),
            base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(200)));
}

TEST_F(NotebookSyncBridgeTest,
       ApplyIncrementalSyncChangesUpdatePreservesCreationTime) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  sync_pb::NotebookSpecifics updated_specifics =
      CreateTestNotebookSpecifics(kTestUuid);
  updated_specifics.set_update_time_windows_epoch_micros(300);
  syncer::EntityData updated_data;
  *updated_data.specifics.mutable_notebook() = updated_specifics;
  updated_data.name = kTestUuid;

  syncer::EntityChangeList update_changes;
  update_changes.push_back(
      syncer::EntityChange::CreateUpdate(kTestUuid, std::move(updated_data)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(update_changes));

  std::optional<Notebook> notebook = model().GetNotebook(
      NotebookId(base::Uuid::ParseCaseInsensitive(kTestUuid)));
  ASSERT_TRUE(notebook.has_value());
  EXPECT_EQ(notebook->creation_time(),
            base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(100)));
}

TEST_F(NotebookSyncBridgeTest,
       ApplyIncrementalSyncChangesUpdateModifiesUpdateTime) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  sync_pb::NotebookSpecifics updated_specifics =
      CreateTestNotebookSpecifics(kTestUuid);
  updated_specifics.set_update_time_windows_epoch_micros(300);
  syncer::EntityData updated_data;
  *updated_data.specifics.mutable_notebook() = updated_specifics;
  updated_data.name = kTestUuid;

  syncer::EntityChangeList update_changes;
  update_changes.push_back(
      syncer::EntityChange::CreateUpdate(kTestUuid, std::move(updated_data)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(update_changes));

  std::optional<Notebook> notebook = model().GetNotebook(
      NotebookId(base::Uuid::ParseCaseInsensitive(kTestUuid)));
  ASSERT_TRUE(notebook.has_value());
  EXPECT_EQ(notebook->update_time(),
            base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(300)));
}

TEST_F(NotebookSyncBridgeTest, ApplyIncrementalSyncChangesDeletesFromBridge) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete(kTestUuid, syncer::EntityData()));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(delete_changes));

  EXPECT_THAT(bridge().entries_for_testing(), testing::IsEmpty());
}

TEST_F(NotebookSyncBridgeTest, ApplyIncrementalSyncChangesDeletesFromModel) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete(kTestUuid, syncer::EntityData()));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(delete_changes));

  EXPECT_EQ(model().GetAllNotebooks().size(), 0u);
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

TEST_F(NotebookSyncBridgeTest,
       ApplyIncrementalSyncChangesDuplicateAddUpdatesModel) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  sync_pb::NotebookSpecifics updated_specifics =
      CreateTestNotebookSpecifics(kTestUuid);
  updated_specifics.set_update_time_windows_epoch_micros(300);
  syncer::EntityData updated_data;
  *updated_data.specifics.mutable_notebook() = updated_specifics;
  updated_data.name = kTestUuid;

  syncer::EntityChangeList dup_add_changes;
  dup_add_changes.push_back(
      syncer::EntityChange::CreateAdd(kTestUuid, std::move(updated_data)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(dup_add_changes));

  std::optional<Notebook> notebook = model().GetNotebook(
      NotebookId(base::Uuid::ParseCaseInsensitive(kTestUuid)));
  ASSERT_TRUE(notebook.has_value());
  EXPECT_EQ(notebook->update_time(),
            base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(300)));
}

TEST_F(NotebookSyncBridgeTest, ApplyDisableSyncChangesClearsBridge) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());

  EXPECT_EQ(bridge().entries_for_testing().size(), 0u);
}

TEST_F(NotebookSyncBridgeTest, ApplyDisableSyncChangesClearsModel) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestUuid, CreateTestEntityData(kTestUuid)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());

  EXPECT_EQ(model().GetAllNotebooks().size(), 0u);
}
}  // namespace

}  // namespace notebooks
