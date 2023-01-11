// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/offline_pages/task/test_task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
using TaskState = TestTask::TaskState;

class ClosureStub {
 public:
  base::OnceClosure Bind() {
    return base::BindOnce(&ClosureStub::Done, base::Unretained(this));
  }
  bool called() const { return called_; }

 private:
  void Done() { called_ = true; }
  bool called_ = false;
};

class NestingTask : public Task {
 public:
  explicit NestingTask(NestingTask* child) : child_(child) {}
  void Run() override {
    if (child_) {
      child_->Execute(
          base::BindOnce(&NestingTask::NestedTaskDone, base::Unretained(this)));
    } else {
      completed_ = true;
      TaskComplete();
    }
  }

  bool completed() const { return completed_; }

 private:
  void NestedTaskDone() {
    completed_ = true;
    TaskComplete();
  }

  raw_ptr<NestingTask> child_ = nullptr;
  bool completed_ = false;
};

class OfflineTaskTest : public testing::Test {
 public:
  OfflineTaskTest();

  void PumpLoop();
 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

OfflineTaskTest::OfflineTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_current_default_handle_(task_runner_) {}

void OfflineTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(OfflineTaskTest, RunTaskStepByStep) {
  ConsumedResource resource;
  TestTask task(&resource);

  EXPECT_EQ(TaskState::NOT_STARTED, task.state());
  ClosureStub complete;
  task.Execute(complete.Bind());
  EXPECT_EQ(TaskState::STEP_1, task.state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();
  EXPECT_EQ(TaskState::STEP_2, task.state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();
  EXPECT_EQ(TaskState::COMPLETED, task.state());
  PumpLoop();
  EXPECT_TRUE(complete.called());
}

TEST_F(OfflineTaskTest, LeaveEarly) {
  ConsumedResource resource;
  TestTask task(&resource, true /* leave early */);
  EXPECT_EQ(TaskState::NOT_STARTED, task.state());
  task.Execute(base::DoNothing());
  EXPECT_EQ(TaskState::STEP_1, task.state());
  EXPECT_TRUE(resource.HasNextStep());
  resource.CompleteStep();

  // Notice STEP_2 was omitted and task went from STEP_1 to completed.
  EXPECT_EQ(TaskState::COMPLETED, task.state());
  EXPECT_FALSE(resource.HasNextStep());
}

TEST_F(OfflineTaskTest, RunNestedTask) {
  NestingTask child(nullptr);
  NestingTask parent(&child);

  parent.Execute(base::DoNothing());
  EXPECT_TRUE(child.completed());
  EXPECT_TRUE(parent.completed());
}

}  // namespace
}  // namespace offline_pages
