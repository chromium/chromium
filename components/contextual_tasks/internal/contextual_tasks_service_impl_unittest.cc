// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/sessions/core/session_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class ContextualTasksServiceImplTest : public testing::Test {
 public:
  ContextualTasksServiceImplTest() = default;
  ~ContextualTasksServiceImplTest() override = default;

 protected:
  ContextualTasksServiceImpl service_;
};

TEST_F(ContextualTasksServiceImplTest, CreateTask) {
  ContextualTask task = service_.CreateTask();
  EXPECT_TRUE(task.GetTaskId().is_valid());

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task.GetTaskId(), tasks[0].GetTaskId());

  service_.DeleteTask(task.GetTaskId());
  tasks = service_.GetTasks();
  EXPECT_TRUE(tasks.empty());
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
  ContextualTask task = service_.CreateTask();
  EXPECT_EQ(1u, service_.GetTasks().size());

  SessionID session_id = SessionID::FromSerializedValue(1);
  service_.AttachSessionIdToTask(task.GetTaskId(), session_id);
  EXPECT_TRUE(service_.GetMostRecentContextualTaskForSessionID(session_id));

  service_.DeleteTask(task.GetTaskId());
  EXPECT_TRUE(service_.GetTasks().empty());
  EXPECT_FALSE(service_.GetMostRecentContextualTaskForSessionID(session_id));
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

TEST_F(ContextualTasksServiceImplTest, AssignServerIdToTask) {
  ContextualTask task = service_.CreateTask();
  ChatType type = ChatType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";

  service_.AssignServerIdToTask(task.GetTaskId(), type, server_id, title);

  std::optional<ContextualTask> result = service_.GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Chat> chat = result->GetChat();
  ASSERT_TRUE(chat.has_value());
  EXPECT_EQ(server_id, chat->server_id);
  EXPECT_EQ(type, chat->type);
  EXPECT_EQ(title, chat->title);
}

TEST_F(ContextualTasksServiceImplTest, AssignAndRemoveServerId_MultipleTasks) {
  ContextualTask task1 = service_.CreateTask();
  ContextualTask task2 = service_.CreateTask();
  ChatType type = ChatType::kAiMode;
  std::string server_id1 = "server_id1";
  std::string server_id2 = "server_id2";
  std::string title1 = "foo1";
  std::string title2 = "foo2";

  service_.AssignServerIdToTask(task1.GetTaskId(), type, server_id1, title1);
  service_.AssignServerIdToTask(task2.GetTaskId(), type, server_id2, title2);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(2u, tasks.size());

  ContextualTask result_task1 =
      tasks[0].GetTaskId() == task1.GetTaskId() ? tasks[0] : tasks[1];
  ContextualTask result_task2 =
      tasks[0].GetTaskId() == task2.GetTaskId() ? tasks[0] : tasks[1];

  std::optional<Chat> chat1 = result_task1.GetChat();
  ASSERT_TRUE(chat1.has_value());
  EXPECT_EQ(server_id1, chat1->server_id);
  EXPECT_EQ(title1, chat1->title);

  std::optional<Chat> chat2 = result_task2.GetChat();
  ASSERT_TRUE(chat2.has_value());
  EXPECT_EQ(server_id2, chat2->server_id);
  EXPECT_EQ(title2, chat2->title);

  service_.RemoveServerIdFromTask(task1.GetTaskId(), type, server_id1);
  tasks = service_.GetTasks();
  ASSERT_EQ(2u, tasks.size());

  result_task1 =
      tasks[0].GetTaskId() == task1.GetTaskId() ? tasks[0] : tasks[1];
  result_task2 =
      tasks[0].GetTaskId() == task2.GetTaskId() ? tasks[0] : tasks[1];

  EXPECT_FALSE(result_task1.GetChat().has_value());
  EXPECT_TRUE(result_task2.GetChat().has_value());
}

TEST_F(ContextualTasksServiceImplTest, RemoveServerIdFromTask) {
  ContextualTask task = service_.CreateTask();
  ChatType type = ChatType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";

  service_.AssignServerIdToTask(task.GetTaskId(), type, server_id, title);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_TRUE(tasks[0].GetChat().has_value());

  service_.RemoveServerIdFromTask(task.GetTaskId(), type, server_id);
  tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_FALSE(tasks[0].GetChat().has_value());

  // Calling remove again should be a no-op and not crash.
  service_.RemoveServerIdFromTask(task.GetTaskId(), type, server_id);
  tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_FALSE(tasks[0].GetChat().has_value());
}

TEST_F(ContextualTasksServiceImplTest, AssignServerIdToTask_TaskDoesNotExist) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ChatType type = ChatType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";

  service_.AssignServerIdToTask(task_id, type, server_id, title);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task_id, tasks[0].GetTaskId());
  std::optional<Chat> chat = tasks[0].GetChat();
  ASSERT_TRUE(chat.has_value());
  EXPECT_EQ(server_id, chat->server_id);
  EXPECT_EQ(type, chat->type);
  EXPECT_EQ(title, chat->title);
}

TEST_F(ContextualTasksServiceImplTest, AttachUrlToTask) {
  ContextualTask task = service_.CreateTask();
  GURL url("https://www.google.com");

  service_.AttachUrlToTask(task.GetTaskId(), url);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  std::vector<GURL> urls = tasks[0].GetUrls();
  ASSERT_EQ(1u, urls.size());
  EXPECT_EQ(url, urls[0]);
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
  ContextualTask task = service_.CreateTask();
  GURL url("https://www.google.com");

  service_.AttachUrlToTask(task.GetTaskId(), url);
  EXPECT_EQ(1u, service_.GetTasks()[0].GetUrls().size());

  service_.DetachUrlFromTask(task.GetTaskId(), url);
  EXPECT_TRUE(service_.GetTasks()[0].GetUrls().empty());
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
