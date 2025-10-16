// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/ai_thread_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

namespace {

using testing::_;
using testing::ReturnRef;

class AiThreadSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics(_))
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    bridge_ = std::make_unique<AiThreadSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<AiThreadSyncBridge> bridge_;
};

TEST_F(AiThreadSyncBridgeTest, GetDataForCommit) {
  syncer::EntityChangeList entity_change_list;
  sync_pb::AiThreadSpecifics specifics;
  specifics.set_server_id("server_id_1");
  specifics.set_title("Title 1");
  specifics.set_conversation_turn_id("turn_id_1");
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_ai_thread() = specifics;
  entity_change_list.push_back(
      syncer::EntityChange::CreateAdd("server_id_1", std::move(entity_data)));

  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(entity_change_list));

  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge_->GetDataForCommit({"server_id_1"});
  ASSERT_TRUE(data_batch);
  ASSERT_TRUE(data_batch->HasNext());
  auto [key, data] = data_batch->Next();
  EXPECT_EQ(key, "server_id_1");
  EXPECT_EQ(data->specifics.ai_thread().title(), "Title 1");
}

TEST_F(AiThreadSyncBridgeTest, GetClientTagAndStorageKey) {
  sync_pb::AiThreadSpecifics specifics;
  specifics.set_server_id("server_id_1");
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_ai_thread() = specifics;

  EXPECT_EQ(bridge_->GetClientTag(entity_data), "server_id_1");
  EXPECT_EQ(bridge_->GetStorageKey(entity_data), "server_id_1");
}

TEST_F(AiThreadSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::AiThreadSpecifics* ai_thread_specifics =
      specifics.mutable_ai_thread();
  ai_thread_specifics->set_server_id("server_id_1");
  ai_thread_specifics->set_title("Title 1");
  ai_thread_specifics->set_conversation_turn_id("turn_id_1");

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(specifics);
  EXPECT_FALSE(trimmed_specifics.ai_thread().has_server_id());
  EXPECT_FALSE(trimmed_specifics.ai_thread().has_title());
  EXPECT_FALSE(trimmed_specifics.ai_thread().has_conversation_turn_id());
}

TEST_F(AiThreadSyncBridgeTest, MergeFullSyncDataAndGetAllData) {
  syncer::EntityChangeList entity_change_list;
  sync_pb::AiThreadSpecifics specifics1;
  specifics1.set_server_id("server_id_1");
  specifics1.set_title("Title 1");
  specifics1.set_conversation_turn_id("turn_id_1");
  syncer::EntityData entity_data1;
  *entity_data1.specifics.mutable_ai_thread() = specifics1;
  entity_change_list.push_back(
      syncer::EntityChange::CreateAdd("server_id_1", std::move(entity_data1)));

  sync_pb::AiThreadSpecifics specifics2;
  specifics2.set_server_id("server_id_2");
  specifics2.set_title("Title 2");
  specifics2.set_conversation_turn_id("turn_id_2");
  syncer::EntityData entity_data2;
  *entity_data2.specifics.mutable_ai_thread() = specifics2;
  entity_change_list.push_back(
      syncer::EntityChange::CreateAdd("server_id_2", std::move(entity_data2)));

  std::optional<syncer::ModelError> error = bridge_->MergeFullSyncData(
      bridge_->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);

  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge_->GetAllDataForDebugging();
  ASSERT_TRUE(data_batch);

  std::map<std::string, std::string> results;
  while (data_batch->HasNext()) {
    auto [key, data] = data_batch->Next();
    results[key] = data->specifics.ai_thread().title();
  }

  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(results["server_id_1"], "Title 1");
  EXPECT_EQ(results["server_id_2"], "Title 2");
}

TEST_F(AiThreadSyncBridgeTest, ApplyIncrementalSyncChanges) {
  syncer::EntityChangeList add_changes;
  sync_pb::AiThreadSpecifics specifics;
  specifics.set_server_id("server_id_1");
  specifics.set_title("Title 1");
  specifics.set_conversation_turn_id("turn_id_1");
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_ai_thread() = specifics;
  add_changes.push_back(
      syncer::EntityChange::CreateAdd("server_id_1", std::move(entity_data)));

  std::optional<syncer::ModelError> error =
      bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                           std::move(add_changes));
  EXPECT_FALSE(error);
  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge_->GetAllDataForDebugging();
  ASSERT_TRUE(data_batch);
  EXPECT_TRUE(data_batch->HasNext());
  auto [key, data] = data_batch->Next();
  EXPECT_EQ(key, "server_id_1");
  EXPECT_EQ(data->specifics.ai_thread().title(), "Title 1");

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete("server_id_1", syncer::EntityData()));
  error = bridge_->ApplyIncrementalSyncChanges(
      bridge_->CreateMetadataChangeList(), std::move(delete_changes));
  EXPECT_FALSE(error);

  data_batch = bridge_->GetAllDataForDebugging();
  ASSERT_TRUE(data_batch);
  EXPECT_FALSE(data_batch->HasNext());
}

TEST_F(AiThreadSyncBridgeTest, GetThread) {
  syncer::EntityChangeList entity_change_list;
  sync_pb::AiThreadSpecifics specifics;
  specifics.set_server_id("server_id_1");
  specifics.set_title("Title 1");
  specifics.set_conversation_turn_id("turn_id_1");
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_ai_thread() = specifics;
  entity_change_list.push_back(
      syncer::EntityChange::CreateAdd("server_id_1", std::move(entity_data)));

  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(entity_change_list));

  std::optional<Thread> thread = bridge_->GetThread("server_id_1");
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(thread->server_id, "server_id_1");
  EXPECT_EQ(thread->title, "Title 1");
  EXPECT_EQ(thread->conversation_turn_id, "turn_id_1");

  std::optional<Thread> not_found_thread = bridge_->GetThread("not_found");
  EXPECT_FALSE(not_found_thread.has_value());
}

}  // namespace

}  // namespace contextual_tasks
