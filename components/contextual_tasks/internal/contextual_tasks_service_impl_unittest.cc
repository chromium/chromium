// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class MockContextualTasksObserver : public ContextualTasksService::Observer {
 public:
  MOCK_METHOD(void,
              OnTaskAdded,
              (const ContextualTask& task,
               ContextualTasksService::TriggerSource source),
              (override));
  MOCK_METHOD(void,
              OnTaskUpdated,
              (const ContextualTask& task,
               ContextualTasksService::TriggerSource source),
              (override));
  MOCK_METHOD(void,
              OnTaskRemoved,
              (const base::Uuid& task_id,
               ContextualTasksService::TriggerSource source),
              (override));
};

class ContextualTasksServiceImplTest : public testing::Test {
 public:
  ContextualTasksServiceImplTest() = default;
  ~ContextualTasksServiceImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  ContextualTasksServiceImpl service_;
  testing::NiceMock<MockContextualTasksObserver> observer_;
};

TEST_F(ContextualTasksServiceImplTest, CreateTask) {
  service_.AddObserver(&observer_);

  EXPECT_CALL(
      observer_,
      OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kLocal));
  ContextualTask task = service_.CreateTask();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(task.GetTaskId().is_valid());

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task.GetTaskId(), tasks[0].GetTaskId());

  service_.DeleteTask(task.GetTaskId());
  tasks = service_.GetTasks();
  EXPECT_TRUE(tasks.empty());
  service_.RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, GetTaskById) {
  ContextualTask task = service_.CreateTask();
  std::optional<ContextualTask> result = service_.GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(task.GetTaskId(), result->GetTaskId());
}

TEST_F(ContextualTasksServiceImplTest, GetTaskById_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  std::optional<ContextualTask> result = service_.GetTaskById(task_id);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContextualTasksServiceImplTest, CreateAndRemoveMultipleTasks) {
  ContextualTask task1 = service_.CreateTask();
  ContextualTask task2 = service_.CreateTask();
  EXPECT_EQ(2u, service_.GetTasks().size());

  service_.DeleteTask(task1.GetTaskId());
  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task2.GetTaskId(), tasks[0].GetTaskId());

  service_.DeleteTask(task2.GetTaskId());
  EXPECT_TRUE(service_.GetTasks().empty());
}

TEST_F(ContextualTasksServiceImplTest, DeleteTask) {
  service_.AddObserver(&observer_);
  ContextualTask task = service_.CreateTask();
  EXPECT_EQ(1u, service_.GetTasks().size());

  SessionID session_id = SessionID::FromSerializedValue(1);
  service_.AttachSessionIdToTask(task.GetTaskId(), session_id);
  EXPECT_TRUE(service_.GetMostRecentContextualTaskForSessionID(session_id));

  EXPECT_CALL(observer_,
              OnTaskRemoved(task.GetTaskId(),
                            ContextualTasksService::TriggerSource::kLocal));
  service_.DeleteTask(task.GetTaskId());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(service_.GetTasks().empty());
  EXPECT_FALSE(service_.GetMostRecentContextualTaskForSessionID(session_id));
  service_.RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, DeleteTask_Twice) {
  ContextualTask task = service_.CreateTask();
  EXPECT_EQ(1u, service_.GetTasks().size());

  service_.DeleteTask(task.GetTaskId());
  EXPECT_TRUE(service_.GetTasks().empty());

  // Calling delete again should be a no-op and not crash.
  service_.DeleteTask(task.GetTaskId());
  EXPECT_TRUE(service_.GetTasks().empty());
}

TEST_F(ContextualTasksServiceImplTest, DeleteTask_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  service_.DeleteTask(task_id);
  EXPECT_TRUE(service_.GetTasks().empty());
}

TEST_F(ContextualTasksServiceImplTest, GetTasks_Empty) {
  std::vector<ContextualTask> tasks = service_.GetTasks();
  EXPECT_TRUE(tasks.empty());
}

TEST_F(ContextualTasksServiceImplTest, AddThreadToTask) {
  service_.AddObserver(&observer_);
  ContextualTask task = service_.CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal));
    service_.AddThreadToTask(
      task.GetTaskId(), Thread(type, server_id, title, conversation_turn_id));
  task_environment_.RunUntilIdle();

  std::optional<ContextualTask> result = service_.GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(type, thread->type);
  EXPECT_EQ(title, thread->title);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_.RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AddAndRemoveThread_MultipleTasks) {
  ContextualTask task1 = service_.CreateTask();
  ContextualTask task2 = service_.CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id1 = "server_id1";
  std::string server_id2 = "server_id2";
  std::string title1 = "foo1";
  std::string title2 = "foo2";
  std::string conversation_turn_id1 = "conversation_turn_id1";
  std::string conversation_turn_id2 = "conversation_turn_id2";

  service_.AddThreadToTask(task1.GetTaskId(), Thread(type, server_id1, title1,
                                                     conversation_turn_id1));
  service_.AddThreadToTask(task2.GetTaskId(), Thread(type, server_id2, title2,
                                                     conversation_turn_id2));

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(2u, tasks.size());

  ContextualTask result_task1 =
      tasks[0].GetTaskId() == task1.GetTaskId() ? tasks[0] : tasks[1];
  ContextualTask result_task2 =
      tasks[0].GetTaskId() == task2.GetTaskId() ? tasks[0] : tasks[1];

  std::optional<Thread> thread1 = result_task1.GetThread();
  ASSERT_TRUE(thread1.has_value());
  EXPECT_EQ(server_id1, thread1->server_id);
  EXPECT_EQ(title1, thread1->title);

  std::optional<Thread> thread2 = result_task2.GetThread();
  ASSERT_TRUE(thread2.has_value());
  EXPECT_EQ(server_id2, thread2->server_id);
  EXPECT_EQ(title2, thread2->title);

  service_.RemoveThreadFromTask(task1.GetTaskId(), type, server_id1);
  tasks = service_.GetTasks();
  ASSERT_EQ(2u, tasks.size());

  result_task1 =
      tasks[0].GetTaskId() == task1.GetTaskId() ? tasks[0] : tasks[1];
  result_task2 =
      tasks[0].GetTaskId() == task2.GetTaskId() ? tasks[0] : tasks[1];

  EXPECT_FALSE(result_task1.GetThread().has_value());
  EXPECT_TRUE(result_task2.GetThread().has_value());
}

TEST_F(ContextualTasksServiceImplTest, RemoveThreadFromTask) {
  service_.AddObserver(&observer_);
  ContextualTask task = service_.CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  service_.AddThreadToTask(
      task.GetTaskId(), Thread(type, server_id, title, conversation_turn_id));
  task_environment_.RunUntilIdle();

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_TRUE(tasks[0].GetThread().has_value());

  service_.RemoveThreadFromTask(task.GetTaskId(), type, server_id);
  task_environment_.RunUntilIdle();
  tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_FALSE(tasks[0].GetThread().has_value());

  // Calling remove again should be a no-op and not crash.
  service_.RemoveThreadFromTask(task.GetTaskId(), type, server_id);
  task_environment_.RunUntilIdle();
  tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_FALSE(tasks[0].GetThread().has_value());
  service_.RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AddThreadToTask_TaskDoesNotExist) {
  service_.AddObserver(&observer_);
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  EXPECT_CALL(
      observer_,
      OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kLocal));
  service_.AddThreadToTask(
      task_id, Thread(type, server_id, title, conversation_turn_id));
  task_environment_.RunUntilIdle();

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task_id, tasks[0].GetTaskId());
  std::optional<Thread> thread = tasks[0].GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(type, thread->type);
  EXPECT_EQ(title, thread->title);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_.RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AttachUrlToTask) {
  service_.AddObserver(&observer_);
  ContextualTask task = service_.CreateTask();
  GURL url("https://www.google.com");

  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal));
  service_.AttachUrlToTask(task.GetTaskId(), url);
  task_environment_.RunUntilIdle();

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  std::vector<GURL> urls = tasks[0].GetUrls();
  ASSERT_EQ(1u, urls.size());
  EXPECT_EQ(url, urls[0]);
  service_.RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AttachAndDetachUrl_MultipleTasks) {
  ContextualTask task1 = service_.CreateTask();
  ContextualTask task2 = service_.CreateTask();
  GURL url1("https://www.google.com");
  GURL url2("https://www.youtube.com");

  service_.AttachUrlToTask(task1.GetTaskId(), url1);
  service_.AttachUrlToTask(task2.GetTaskId(), url2);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(2u, tasks.size());

  ContextualTask result_task1 =
      tasks[0].GetTaskId() == task1.GetTaskId() ? tasks[0] : tasks[1];
  ContextualTask result_task2 =
      tasks[0].GetTaskId() == task2.GetTaskId() ? tasks[0] : tasks[1];

  std::vector<GURL> urls1 = result_task1.GetUrls();
  ASSERT_EQ(1u, urls1.size());
  EXPECT_EQ(url1, urls1[0]);

  std::vector<GURL> urls2 = result_task2.GetUrls();
  ASSERT_EQ(1u, urls2.size());
  EXPECT_EQ(url2, urls2[0]);

  service_.DetachUrlFromTask(task1.GetTaskId(), url1);
  tasks = service_.GetTasks();
  ASSERT_EQ(2u, tasks.size());

  result_task1 =
      tasks[0].GetTaskId() == task1.GetTaskId() ? tasks[0] : tasks[1];
  result_task2 =
      tasks[0].GetTaskId() == task2.GetTaskId() ? tasks[0] : tasks[1];

  EXPECT_TRUE(result_task1.GetUrls().empty());
  EXPECT_EQ(1u, result_task2.GetUrls().size());
}

TEST_F(ContextualTasksServiceImplTest, DetachUrlFromTask) {
  service_.AddObserver(&observer_);
  ContextualTask task = service_.CreateTask();
  GURL url("https://www.google.com");

  service_.AttachUrlToTask(task.GetTaskId(), url);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, service_.GetTasks()[0].GetUrls().size());

  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal));
  service_.DetachUrlFromTask(task.GetTaskId(), url);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(service_.GetTasks()[0].GetUrls().empty());
  service_.RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AttachSessionIdToTask) {
  ContextualTask task = service_.CreateTask();
  SessionID session_id = SessionID::FromSerializedValue(1);

  service_.AttachSessionIdToTask(task.GetTaskId(), session_id);

  std::optional<ContextualTask> recent_task =
      service_.GetMostRecentContextualTaskForSessionID(session_id);
  ASSERT_TRUE(recent_task.has_value());
  EXPECT_EQ(task.GetTaskId(), recent_task->GetTaskId());
}

TEST_F(ContextualTasksServiceImplTest, AttachSessionIdToInvalidTask) {
  ContextualTask task = service_.CreateTask();
  SessionID session_id = SessionID::FromSerializedValue(1);
  base::Uuid task_id = task.GetTaskId();
  service_.DeleteTask(task_id);

  // The session Id is not added, as the task is deleted.
  service_.AttachSessionIdToTask(task_id, session_id);

  std::optional<ContextualTask> recent_task =
      service_.GetMostRecentContextualTaskForSessionID(session_id);
  ASSERT_FALSE(recent_task.has_value());
  EXPECT_EQ(0u, service_.GetSessionIdMapSizeForTesting());
}

TEST_F(ContextualTasksServiceImplTest, DetachSessionIdFromTask) {
  ContextualTask task = service_.CreateTask();
  SessionID session_id = SessionID::FromSerializedValue(1);

  service_.AttachSessionIdToTask(task.GetTaskId(), session_id);
  EXPECT_TRUE(service_.GetMostRecentContextualTaskForSessionID(session_id));

  service_.DetachSessionIdFromTask(task.GetTaskId(), session_id);
  EXPECT_FALSE(service_.GetMostRecentContextualTaskForSessionID(session_id));
}

TEST_F(ContextualTasksServiceImplTest,
       GetMostRecentContextualTaskForSessionID_NotFound) {
  SessionID session_id = SessionID::FromSerializedValue(1);
  std::optional<ContextualTask> recent_task =
      service_.GetMostRecentContextualTaskForSessionID(session_id);
  EXPECT_FALSE(recent_task.has_value());
}

}  // namespace contextual_tasks
