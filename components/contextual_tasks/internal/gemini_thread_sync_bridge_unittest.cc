// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/gemini_thread_sync_bridge.h"

#include "base/test/task_environment.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/gemini_thread_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kMockConversationId[] = "my_conversation_id";
const char kMockTitle[] = "my_title";
const int64_t kMockLastTurnTimeUnixEpochMillis = 1771020815;

void AddTestGeminiSpecifics(contextual_tasks::GeminiThreadSyncBridge* bridge) {
  syncer::EntityChangeList add_changes;
  sync_pb::GeminiThreadSpecifics specifics;
  specifics.set_conversation_id(kMockConversationId);
  specifics.set_title(kMockTitle);
  specifics.set_last_turn_time_unix_epoch_millis(
      kMockLastTurnTimeUnixEpochMillis);
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_gemini_thread() = specifics;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kMockConversationId, std::move(entity_data)));

  std::optional<syncer::ModelError> error = bridge->ApplyIncrementalSyncChanges(
      bridge->CreateMetadataChangeList(), std::move(add_changes));
  EXPECT_FALSE(error);
}

void VerifyTestGeminiSpecifics(
    contextual_tasks::GeminiThreadSyncBridge* bridge) {
  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge->GetAllDataForDebugging();
  ASSERT_TRUE(data_batch);
  EXPECT_TRUE(data_batch->HasNext());
  auto [key, data] = data_batch->Next();
  EXPECT_EQ(key, kMockConversationId);
  EXPECT_EQ(data->specifics.gemini_thread().title(), kMockTitle);
  EXPECT_EQ(data->specifics.gemini_thread().last_turn_time_unix_epoch_millis(),
            kMockLastTurnTimeUnixEpochMillis);

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete(
      kMockConversationId, syncer::EntityData()));
  std::optional<syncer::ModelError> error = bridge->ApplyIncrementalSyncChanges(
      bridge->CreateMetadataChangeList(), std::move(delete_changes));
  EXPECT_FALSE(error);

  data_batch = bridge->GetAllDataForDebugging();
  ASSERT_TRUE(data_batch);
  EXPECT_FALSE(data_batch->HasNext());
}

void VerifyTestGeminiSpecificsRemoved(
    contextual_tasks::GeminiThreadSyncBridge* bridge) {
  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge->GetAllDataForDebugging();
  ASSERT_TRUE(data_batch);
  EXPECT_FALSE(data_batch->HasNext());
}

}  // namespace

namespace contextual_tasks {

namespace {

class GeminiThreadSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    bridge_ = std::make_unique<GeminiThreadSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
  }

  GeminiThreadSyncBridge* bridge() { return bridge_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<GeminiThreadSyncBridge> bridge_;
};

TEST_F(GeminiThreadSyncBridgeTest, ApplyIncrementalSyncChanges) {
  AddTestGeminiSpecifics(bridge());
  VerifyTestGeminiSpecifics(bridge());
}

TEST_F(GeminiThreadSyncBridgeTest, DisableSyncChanges) {
  AddTestGeminiSpecifics(bridge());
  VerifyTestGeminiSpecifics(bridge());
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  VerifyTestGeminiSpecificsRemoved(bridge());
}

}  // namespace

}  // namespace contextual_tasks
