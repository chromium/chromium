// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

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

}  // namespace contextual_tasks
