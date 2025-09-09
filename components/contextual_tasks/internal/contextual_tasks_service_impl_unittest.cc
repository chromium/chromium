// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  service_.DeleteTask(task.GetTaskId());
  EXPECT_TRUE(service_.GetTasks().empty());
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

  service_.AssignServerIdToTask(task.GetTaskId(), type, server_id);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  std::optional<Chat> chat = tasks[0].GetChat();
  ASSERT_TRUE(chat.has_value());
  EXPECT_EQ(server_id, chat->server_id);
  EXPECT_EQ(type, chat->type);
}

TEST_F(ContextualTasksServiceImplTest, AssignAndRemoveServerId_MultipleTasks) {
  ContextualTask task1 = service_.CreateTask();
  ContextualTask task2 = service_.CreateTask();
  ChatType type = ChatType::kAiMode;
  std::string server_id1 = "server_id1";
  std::string server_id2 = "server_id2";

  service_.AssignServerIdToTask(task1.GetTaskId(), type, server_id1);
  service_.AssignServerIdToTask(task2.GetTaskId(), type, server_id2);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(2u, tasks.size());

  ContextualTask result_task1 =
      tasks[0].GetTaskId() == task1.GetTaskId() ? tasks[0] : tasks[1];
  ContextualTask result_task2 =
      tasks[0].GetTaskId() == task2.GetTaskId() ? tasks[0] : tasks[1];

  std::optional<Chat> chat1 = result_task1.GetChat();
  ASSERT_TRUE(chat1.has_value());
  EXPECT_EQ(server_id1, chat1->server_id);

  std::optional<Chat> chat2 = result_task2.GetChat();
  ASSERT_TRUE(chat2.has_value());
  EXPECT_EQ(server_id2, chat2->server_id);

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

  service_.AssignServerIdToTask(task.GetTaskId(), type, server_id);

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

  service_.AssignServerIdToTask(task_id, type, server_id);

  std::vector<ContextualTask> tasks = service_.GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task_id, tasks[0].GetTaskId());
  std::optional<Chat> chat = tasks[0].GetChat();
  ASSERT_TRUE(chat.has_value());
  EXPECT_EQ(server_id, chat->server_id);
  EXPECT_EQ(type, chat->type);
}

}  // namespace contextual_tasks
