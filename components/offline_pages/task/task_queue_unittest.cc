// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task_queue.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/task/test_task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using TaskState = TestTask::TaskState;

class OfflineTaskQueueTest : public testing::Test, public TaskQueue::Delegate {
 public:
  OfflineTaskQueueTest();
  ~OfflineTaskQueueTest() override = default;

  // TaskQueue::Delegate
  void OnTaskQueueIsIdle() override;

  void TaskCompleted(Task* task);
  void PumpLoop();

  Task* completed_task() const { return completed_task_; }
  bool on_idle_called() { return on_idle_called_; }

 private:
  Task* completed_task_ = nullptr;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  bool on_idle_called_ = false;
};

OfflineTaskQueueTest::OfflineTaskQueueTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

void OfflineTaskQueueTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflineTaskQueueTest::OnTaskQueueIsIdle() {
  on_idle_called_ = true;
}

void OfflineTaskQueueTest::TaskCompleted(Task* task) {
  completed_task_ = task;
}

TEST_F(OfflineTaskQueueTest, AddAndRunSingleTask) {
  ConsumedResource resource;
  std::unique_ptr<TestTask> task(new TestTask(&resource));
  TestTask* task_ptr = task.get();
  TaskQueue queue(this);
  EXPECT_FALSE(on_idle_called());
  queue.AddTask(std::move(task));
  EXPECT_TRUE(queue.HasPendingTasks());
  EXPECT_TRUE(queue.HasRunningTask());
  EXPECT_EQ(TaskState::NOT_STARTED, task_ptr->state());
  PumpLoop();  // Start running the task.
  EXPECT_EQ(TaskState::STEP_1, task_ptr->state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();

  EXPECT_EQ(TaskState::STEP_2, task_ptr->state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();

  EXPECT_EQ(TaskState::COMPLETED, task_ptr->state());
  EXPECT_FALSE(resource.HasNextStep());
  PumpLoop();  // Deletes task, task_ptr is invalid after that.

  EXPECT_TRUE(on_idle_called());
  EXPECT_FALSE(queue.HasRunningTask());
  EXPECT_FALSE(queue.HasPendingTasks());
}

TEST_F(OfflineTaskQueueTest, AddAndRunMultipleTasks) {
  ConsumedResource resource;
  std::unique_ptr<TestTask> task_1(new TestTask(&resource));
  TestTask* task_1_ptr = task_1.get();
  std::unique_ptr<TestTask> task_2(new TestTask(&resource));
  TestTask* task_2_ptr = task_2.get();

  TaskQueue queue(this);
  queue.AddTask(std::move(task_1));
  queue.AddTask(std::move(task_2));
  EXPECT_TRUE(queue.HasPendingTasks());
  EXPECT_TRUE(queue.HasRunningTask());
  EXPECT_EQ(TaskState::NOT_STARTED, task_1_ptr->state());
  EXPECT_EQ(TaskState::NOT_STARTED, task_2_ptr->state());
  PumpLoop();  // Start running the task 1.
  EXPECT_EQ(TaskState::STEP_1, task_1_ptr->state());
  EXPECT_EQ(TaskState::NOT_STARTED, task_2_ptr->state());
  resource.CompleteStep();

  EXPECT_EQ(TaskState::STEP_2, task_1_ptr->state());
  EXPECT_EQ(TaskState::NOT_STARTED, task_2_ptr->state());
  resource.CompleteStep();

  EXPECT_EQ(TaskState::COMPLETED, task_1_ptr->state());
  EXPECT_EQ(TaskState::NOT_STARTED, task_2_ptr->state());
  PumpLoop();  // Deletes task_1, task_1_ptr is invalid after that.
  EXPECT_EQ(TaskState::STEP_1, task_2_ptr->state());
  EXPECT_FALSE(on_idle_called());
}

TEST_F(OfflineTaskQueueTest, LeaveEarly) {
  ConsumedResource resource;
  std::unique_ptr<TestTask> task(
      new TestTask(&resource, true /* leave early */));
  TestTask* task_ptr = task.get();
  TaskQueue queue(this);
  queue.AddTask(std::move(task));
  EXPECT_TRUE(queue.HasPendingTasks());
  EXPECT_TRUE(queue.HasRunningTask());
  EXPECT_EQ(TaskState::NOT_STARTED, task_ptr->state());
  PumpLoop();  // Start running the task.
  EXPECT_EQ(TaskState::STEP_1, task_ptr->state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();

  // Notice STEP_2 was omitted and task went from STEP_1 to completed.
  EXPECT_EQ(TaskState::COMPLETED, task_ptr->state());
  EXPECT_FALSE(resource.HasNextStep());
  PumpLoop();  // Deletes task, task_ptr is invalid after that.

  EXPECT_TRUE(on_idle_called());
  EXPECT_FALSE(queue.HasPendingTasks());
  EXPECT_FALSE(queue.HasRunningTask());
}

}  // namespace offline_pages
