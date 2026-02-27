// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/gemini_thread_sync_bridge.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/gemini_thread_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kInitConversationId[] = "init_conversation_id";
const char kInitTitle[] = "init_title";
const int64_t kInitLastTurnTimeUnixEpochMillis = 1700000000;
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

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<GeminiThreadSyncBridge> bridge_;
};

class MockObserver : public GeminiThreadSyncBridge::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnGeminiThreadDataStoreLoaded, (), (override));
};

class GeminiThreadSyncBridgeWithInitSpecificsTest
    : public GeminiThreadSyncBridgeTest {
 public:
  void SetUp() override {
    store_ = syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
    AddInitialSpecifics();
    bridge_ = std::make_unique<GeminiThreadSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
    base::RunLoop run_loop;
    bridge_->AddObserver(&observer_);
    ON_CALL(observer_, OnGeminiThreadDataStoreLoaded)
        .WillByDefault(
            testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
  }

  void TearDown() override {
    bridge_->RemoveObserver(&observer_);
    GeminiThreadSyncBridgeTest::TearDown();
  }

  std::unordered_map<std::string, sync_pb::GeminiThreadSpecifics>&
  gemini_thread_specifics() {
    return bridge_->gemini_thread_specifics_for_testing();
  }

 private:
  void AddInitialSpecifics() {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    sync_pb::GeminiThreadSpecifics gemini_specifics;
    gemini_specifics.set_conversation_id(kInitConversationId);
    gemini_specifics.set_title(kInitTitle);
    gemini_specifics.set_last_turn_time_unix_epoch_millis(
        kInitLastTurnTimeUnixEpochMillis);
    batch->WriteData(gemini_specifics.conversation_id(),
                     gemini_specifics.SerializeAsString());
    CommitToStoreAndWait(std::move(batch));
  }

  void CommitToStoreAndWait(
      std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
    base::RunLoop loop;
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            [](base::RunLoop* loop,
               const std::optional<syncer::ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              loop->Quit();
            },
            &loop));
    loop.Run();
  }

  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<MockObserver> observer_;
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

TEST_F(GeminiThreadSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::GeminiThreadSpecifics* gemini_specifics =
      specifics.mutable_gemini_thread();
  gemini_specifics->set_conversation_id(kMockConversationId);
  gemini_specifics->set_title(kMockTitle);
  gemini_specifics->set_last_turn_time_unix_epoch_millis(
      kMockLastTurnTimeUnixEpochMillis);

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(specifics);
  EXPECT_FALSE(trimmed_specifics.ai_thread().has_conversation_turn_id());
  EXPECT_FALSE(trimmed_specifics.ai_thread().has_title());
  EXPECT_FALSE(
      trimmed_specifics.ai_thread().has_last_turn_time_unix_epoch_millis());
}

TEST_F(GeminiThreadSyncBridgeWithInitSpecificsTest, TestReadAllData) {
  EXPECT_TRUE(gemini_thread_specifics().find(kInitConversationId) !=
              gemini_thread_specifics().end());
  sync_pb::GeminiThreadSpecifics gemini_specifics =
      gemini_thread_specifics().find(kInitConversationId)->second;
  EXPECT_EQ(gemini_specifics.conversation_id(), kInitConversationId);
  EXPECT_EQ(gemini_specifics.title(), kInitTitle);
  EXPECT_EQ(gemini_specifics.last_turn_time_unix_epoch_millis(),
            kInitLastTurnTimeUnixEpochMillis);
}

}  // namespace

}  // namespace contextual_tasks
