// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task.h"

#include <memory>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/task/test_task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using TaskState = TestTask::TaskState;

class OfflineTaskTest : public testing::Test {
 public:
  OfflineTaskTest();

  void TaskCompleted(Task* task);
  void PumpLoop();

  Task* completed_task() const { return completed_task_; }

 private:
  Task* completed_task_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

OfflineTaskTest::OfflineTaskTest()
    : completed_task_(nullptr),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

void OfflineTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflineTaskTest::TaskCompleted(Task* task) {
  auto set_task_callback = [](Task** t_ptr, Task* t) { *t_ptr = t; };
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(set_task_callback, &completed_task_, task));
}

TEST_F(OfflineTaskTest, RunTaskStepByStep) {
  ConsumedResource resource;
  TestTask task(&resource);
  task.SetTaskCompletionCallbackForTesting(
      base::BindOnce(&OfflineTaskTest::TaskCompleted, base::Unretained(this)));

  EXPECT_EQ(TaskState::NOT_STARTED, task.state());
  task.Run();
  EXPECT_EQ(TaskState::STEP_1, task.state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();
  EXPECT_EQ(TaskState::STEP_2, task.state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();
  EXPECT_EQ(TaskState::COMPLETED, task.state());
  PumpLoop();
  EXPECT_EQ(completed_task(), &task);
}

TEST_F(OfflineTaskTest, LeaveEarly) {
  ConsumedResource resource;
  TestTask task(&resource, true /* leave early */);
  EXPECT_EQ(TaskState::NOT_STARTED, task.state());
  task.Run();
  EXPECT_EQ(TaskState::STEP_1, task.state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();

  // Notice STEP_2 was omitted and task went from STEP_1 to completed.
  EXPECT_EQ(TaskState::COMPLETED, task.state());
  EXPECT_FALSE(resource.HasNextStep());
}

}  // namespace offline_pages
