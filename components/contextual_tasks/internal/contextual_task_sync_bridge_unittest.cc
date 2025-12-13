// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_task_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

using testing::_;
using testing::ReturnRef;

namespace {

std::unique_ptr<syncer::EntityChange> CreateEntityChange(
    const std::string& guid,
    const std::string& title,
    syncer::EntityChange::ChangeType change_type) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_contextual_task()->set_guid(guid);
  specifics.mutable_contextual_task()->mutable_contextual_task()->set_title(
      title);

  syncer::EntityData entity_data;
  entity_data.specifics = specifics;
  entity_data.name = title;

  if (change_type == syncer::EntityChange::ACTION_DELETE) {
    return syncer::EntityChange::CreateDelete(guid, syncer::EntityData());
  } else if (change_type == syncer::EntityChange::ACTION_UPDATE) {
    return syncer::EntityChange::CreateUpdate(guid, std::move(entity_data));
  }

  return syncer::EntityChange::CreateAdd(guid, std::move(entity_data));
}

class MockObserver : public ContextualTaskSyncBridge::Observer {
 public:
  MOCK_METHOD(void, OnContextualTaskDataStoreLoaded, (), (override));
  MOCK_METHOD(void,
              OnTaskAddedOrUpdatedRemotely,
              (const std::vector<ContextualTask>&),
              (override));
  MOCK_METHOD(void,
              OnTaskRemovedRemotely,
              (const std::vector<base::Uuid>&),
              (override));
};

}  // namespace

class ContextualTaskSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics(_))
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<ContextualTaskSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
    bridge_->AddObserver(&observer_);
  }

  void TearDown() override { bridge_->RemoveObserver(&observer_); }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<ContextualTaskSyncBridge> bridge_;
  MockObserver observer_;
};

TEST_F(ContextualTaskSyncBridgeTest, GetClientTagAndStorageKey) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_contextual_task()->set_guid("guid_1");
  syncer::EntityData entity_data;
  entity_data.specifics = specifics;

  EXPECT_EQ(bridge_->GetClientTag(entity_data), "guid_1");
  EXPECT_EQ(bridge_->GetStorageKey(entity_data), "guid_1");
}

TEST_F(ContextualTaskSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData valid_data;
  valid_data.specifics.mutable_contextual_task()->mutable_contextual_task();
  EXPECT_TRUE(bridge_->IsEntityDataValid(valid_data));

  syncer::EntityData valid_data_url;
  valid_data_url.specifics.mutable_contextual_task()->mutable_url_resource();
  EXPECT_TRUE(bridge_->IsEntityDataValid(valid_data_url));

  syncer::EntityData invalid_data;
  invalid_data.specifics.mutable_web_app();
  EXPECT_FALSE(bridge_->IsEntityDataValid(invalid_data));
}

TEST_F(ContextualTaskSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics specifics;
  auto* task_specifics = specifics.mutable_contextual_task();
  task_specifics->set_guid("guid_1");
  task_specifics->set_version(123);
  auto* task = task_specifics->mutable_contextual_task();
  task->set_title("Test Title");
  task->set_thread_id("thread_id_1");

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(specifics);

  EXPECT_FALSE(trimmed_specifics.contextual_task().has_guid());
  EXPECT_FALSE(trimmed_specifics.contextual_task().has_version());
  EXPECT_FALSE(trimmed_specifics.contextual_task().has_contextual_task());
  EXPECT_FALSE(
      trimmed_specifics.contextual_task().contextual_task().has_title());
  EXPECT_FALSE(
      trimmed_specifics.contextual_task().contextual_task().has_thread_id());
}

TEST_F(ContextualTaskSyncBridgeTest,
       ApplyIncrementalSyncChanges_AddUpdateDelete) {
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateEntityChange("guid_add", "Added Title",
                                           syncer::EntityChange::ACTION_ADD));
  change_list.push_back(CreateEntityChange("guid_existing", "Initial Title",
                                           syncer::EntityChange::ACTION_ADD));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(change_list));

  syncer::EntityChangeList update_list;
  update_list.push_back(CreateEntityChange(
      "guid_existing", "Updated Title", syncer::EntityChange::ACTION_UPDATE));
  update_list.push_back(
      CreateEntityChange("guid_add", "", syncer::EntityChange::ACTION_DELETE));

  EXPECT_CALL(observer_, OnTaskAddedOrUpdatedRemotely(_))
      .WillOnce([&](const std::vector<ContextualTask>& tasks) {
        EXPECT_EQ(1u, tasks.size());
        barrier.Run();
      });
  EXPECT_CALL(observer_, OnTaskRemovedRemotely(_))
      .WillOnce([&](const std::vector<base::Uuid>& guids) {
        EXPECT_EQ(1u, guids.size());
        barrier.Run();
      });

  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(update_list));
  run_loop.Run();
}

TEST_F(ContextualTaskSyncBridgeTest, OnDataTypeStoreLoaded) {
  auto store_factory =
      syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnContextualTaskDataStoreLoaded()).WillOnce([&]() {
    run_loop.Quit();
  });

  bridge_ = std::make_unique<ContextualTaskSyncBridge>(
      mock_processor_.CreateForwardingProcessor(), std::move(store_factory));
  bridge_->AddObserver(&observer_);
  run_loop.Run();
}

TEST_F(ContextualTaskSyncBridgeTest, GetTasks) {
  // Task ID, will be referenced by all the UrlResource to bind to the task.
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  std::string task_id_str = task_id.AsLowercaseString();

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateEntityChange(task_id_str, "Task Title",
                                           syncer::EntityChange::ACTION_ADD));

  sync_pb::EntitySpecifics url_specifics;
  // The top level GUID of UrlResource needs to be unique,
  url_specifics.mutable_contextual_task()->set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  // Bind the url resource specifics to the task through the task ID.
  url_specifics.mutable_contextual_task()
      ->mutable_url_resource()
      ->set_task_guid(task_id_str);
  url_specifics.mutable_contextual_task()->mutable_url_resource()->set_url(
      "https://example.com");
  syncer::EntityData url_entity_data;
  url_entity_data.specifics = url_specifics;
  url_entity_data.name = "url";
  change_list.push_back(syncer::EntityChange::CreateAdd(
      url_specifics.contextual_task().guid(), std::move(url_entity_data)));

  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(change_list));

  std::vector<ContextualTask> tasks = bridge_->GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task_id, tasks[0].GetTaskId());
  EXPECT_EQ("Task Title", tasks[0].GetTitle());
  ASSERT_EQ(1u, tasks[0].GetUrlResources().size());
  EXPECT_EQ(GURL("https://example.com"), tasks[0].GetUrlResources()[0].url);
}

TEST_F(ContextualTaskSyncBridgeTest, GetTaskById) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  std::string task_id_str = task_id.AsLowercaseString();

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateEntityChange(task_id_str, "Task Title",
                                           syncer::EntityChange::ACTION_ADD));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(change_list));

  std::optional<ContextualTask> task = bridge_->GetTaskById(task_id_str);
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task_id, task->GetTaskId());
  EXPECT_EQ("Task Title", task->GetTitle());

  std::optional<ContextualTask> not_found_task =
      bridge_->GetTaskById(base::Uuid::GenerateRandomV4().AsLowercaseString());
  EXPECT_FALSE(not_found_task.has_value());
}

TEST_F(ContextualTaskSyncBridgeTest, OnTaskAddedLocally) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  task.SetTitle("New Task");

  EXPECT_CALL(mock_processor_, Put(_, _, _));
  bridge_->OnTaskAddedLocally(task);

  std::optional<ContextualTask> retrieved_task =
      bridge_->GetTaskById(task_id.AsLowercaseString());
  ASSERT_TRUE(retrieved_task.has_value());
  EXPECT_EQ(task.GetTitle(), retrieved_task->GetTitle());
}

TEST_F(ContextualTaskSyncBridgeTest, OnTaskRemovedLocally) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  task.SetTitle("Task to Remove");
  bridge_->OnTaskAddedLocally(task);

  UrlResource url_resource(base::Uuid::GenerateRandomV4(),
                           GURL("https://example.com"));
  bridge_->OnUrlAddedToTaskLocally(task_id, url_resource);

  ASSERT_TRUE(bridge_->GetTaskById(task_id.AsLowercaseString()).has_value());
  ASSERT_EQ(1u, bridge_->GetTaskById(task_id.AsLowercaseString())
                    ->GetUrlResources()
                    .size());

  EXPECT_CALL(mock_processor_, Delete(task_id.AsLowercaseString(), _, _));
  EXPECT_CALL(mock_processor_,
              Delete(url_resource.url_id.AsLowercaseString(), _, _));
  bridge_->OnTaskRemovedLocally(task_id);

  EXPECT_FALSE(bridge_->GetTaskById(task_id.AsLowercaseString()).has_value());
}

TEST_F(ContextualTaskSyncBridgeTest, OnTaskUpdatedLocally) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  task.SetTitle("Initial Title");

  bridge_->OnTaskAddedLocally(task);
  task.SetTitle("Updated Title");

  EXPECT_CALL(mock_processor_, Put(_, _, _));
  bridge_->OnTaskUpdatedLocally(task);

  std::optional<ContextualTask> retrieved_task =
      bridge_->GetTaskById(task_id.AsLowercaseString());
  ASSERT_TRUE(retrieved_task.has_value());
  EXPECT_EQ("Updated Title", retrieved_task->GetTitle());
}

TEST_F(ContextualTaskSyncBridgeTest, OnUrlAddedToTask) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  bridge_->OnTaskAddedLocally(task);

  UrlResource url_resource(base::Uuid::GenerateRandomV4(),
                           GURL("https://example.com"));

  EXPECT_CALL(mock_processor_, Put(_, _, _));
  bridge_->OnUrlAddedToTaskLocally(task_id, url_resource);

  std::optional<ContextualTask> retrieved_task =
      bridge_->GetTaskById(task_id.AsLowercaseString());
  ASSERT_TRUE(retrieved_task.has_value());
  ASSERT_EQ(1u, retrieved_task->GetUrlResources().size());
  EXPECT_EQ(url_resource.url, retrieved_task->GetUrlResources()[0].url);
}

TEST_F(ContextualTaskSyncBridgeTest, OnUrlRemovedFromTask) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  bridge_->OnTaskAddedLocally(task);

  UrlResource url_resource(base::Uuid::GenerateRandomV4(),
                           GURL("https://example.com"));
  bridge_->OnUrlAddedToTaskLocally(task_id, url_resource);

  ASSERT_EQ(1u, bridge_->GetTaskById(task_id.AsLowercaseString())
                    ->GetUrlResources()
                    .size());

  EXPECT_CALL(mock_processor_, Delete(_, _, _));
  bridge_->OnUrlRemovedFromTaskLocally(url_resource.url_id);

  std::optional<ContextualTask> retrieved_task =
      bridge_->GetTaskById(task_id.AsLowercaseString());
  ASSERT_TRUE(retrieved_task.has_value());
  EXPECT_TRUE(retrieved_task->GetUrlResources().empty());
}

TEST_F(ContextualTaskSyncBridgeTest, EphemralTasksAreNotPersisted) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id, /*is_ephemeral=*/true);
  task.SetTitle("New Task");

  EXPECT_CALL(mock_processor_, Put(_, _, _)).Times(0);
  bridge_->OnTaskAddedLocally(task);

  std::optional<ContextualTask> retrieved_task =
      bridge_->GetTaskById(task_id.AsLowercaseString());
  ASSERT_FALSE(retrieved_task.has_value());

  bridge_->OnTaskUpdatedLocally(task);
  retrieved_task = bridge_->GetTaskById(task_id.AsLowercaseString());
  ASSERT_FALSE(retrieved_task.has_value());
}

}  // namespace contextual_tasks
