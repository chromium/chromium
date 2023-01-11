// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task_queue.h"

#include <memory>
#include <utility>
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/offline_pages/task/test_task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class SimpleTask : public Task {
 public:
  SimpleTask(const std::string& name, std::vector<std::string>* events)
      : events_(events), name_(name) {}

  void Run() override {
    events_->push_back("Run " + name_);
    TaskComplete();
  }
  using Task::TaskComplete;

 protected:
  raw_ptr<std::vector<std::string>> events_;
  std::string name_;
};

// A test task which suspends and waits for something to complete before
// resuming.
class SuspendingTask : public SimpleTask {
 public:
  SuspendingTask(const std::string& name,
                 std::vector<std::string>* events,
                 int suspend_count = 1)
      : SimpleTask(name, events), suspend_count_(suspend_count) {}
  ~SuspendingTask() override { events_->push_back("Destroy " + name_); }
  void Run() override {
    events_->push_back("Run " + name_);
    DoWork();
  }

  void DoResume() {
    events_->push_back("DoResume " + name_);
    Resume(
        base::BindOnce(&SuspendingTask::TaskResumed, base::Unretained(this)));
  }

  void TaskResumed() {
    events_->push_back("TaskResumed " + name_);
    DoWork();
  }

  void Done() { TaskComplete(); }

 private:
  void DoWork() {
    if (suspend_count_ > 0) {
      --suspend_count_;
      Suspend();
      return;
    }
    Done();
  }
  int suspend_count_ = 0;
};

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
  raw_ptr<Task> completed_task_ = nullptr;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
  bool on_idle_called_ = false;
};

OfflineTaskQueueTest::OfflineTaskQueueTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_current_default_handle_(task_runner_) {}

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

TEST_F(OfflineTaskQueueTest, SuspendAndResume) {
  TaskQueue queue(this);
  std::vector<std::string> events;
  queue.AddTask(std::make_unique<SimpleTask>("T1", &events));
  SuspendingTask* t2;
  {
    auto task = std::make_unique<SuspendingTask>("T2", &events);
    t2 = task.get();
    queue.AddTask(std::move(task));
  }
  PumpLoop();
  queue.AddTask(std::make_unique<SimpleTask>("T3", &events));
  PumpLoop();
  queue.AddTask(std::make_unique<SimpleTask>("T4", &events));
  t2->DoResume();
  queue.AddTask(std::make_unique<SimpleTask>("T5", &events));
  PumpLoop();

  EXPECT_EQ(std::vector<std::string>({"Run T1", "Run T2", "Run T3",
                                      "DoResume T2", "Run T4", "TaskResumed T2",
                                      "Destroy T2", "Run T5"}),
            events);
}

TEST_F(OfflineTaskQueueTest, SuspendResumeSuspend) {
  TaskQueue queue(this);
  std::vector<std::string> events;
  queue.AddTask(std::make_unique<SimpleTask>("T1", &events));
  SuspendingTask* t2;
  {
    auto task =
        std::make_unique<SuspendingTask>("T2", &events, /*suspend_count=*/2);
    t2 = task.get();
    queue.AddTask(std::move(task));
  }
  PumpLoop();
  t2->DoResume();
  PumpLoop();
  t2->DoResume();
  PumpLoop();

  EXPECT_EQ(std::vector<std::string>({"Run T1", "Run T2", "DoResume T2",
                                      "TaskResumed T2", "DoResume T2",
                                      "TaskResumed T2", "Destroy T2"}),
            events);
}

TEST_F(OfflineTaskQueueTest, SuspendAndResumeNoOtherTasksRunning) {
  TaskQueue queue(this);
  std::vector<std::string> events;

  SuspendingTask* t1;
  {
    auto task = std::make_unique<SuspendingTask>("T1", &events);
    t1 = task.get();
    queue.AddTask(std::move(task));
  }
  PumpLoop();
  t1->DoResume();
  PumpLoop();

  EXPECT_EQ(std::vector<std::string>(
                {"Run T1", "DoResume T1", "TaskResumed T1", "Destroy T1"}),
            events);
}

TEST_F(OfflineTaskQueueTest, SuspendAndNeverResume) {
  std::vector<std::string> events;
  {
    TaskQueue queue(this);
    queue.AddTask(std::make_unique<SuspendingTask>("T1", &events));
    PumpLoop();
  }

  EXPECT_EQ(std::vector<std::string>({"Run T1", "Destroy T1"}), events);
}

TEST_F(OfflineTaskQueueTest, CompleteWhileSuspended) {
  TaskQueue queue(this);
  std::vector<std::string> events;
  SuspendingTask* t1;
  {
    auto task = std::make_unique<SuspendingTask>("T1", &events);
    t1 = task.get();
    queue.AddTask(std::move(task));
  }
  PumpLoop();
  t1->Done();
  PumpLoop();
  EXPECT_EQ(std::vector<std::string>({"Run T1", "Destroy T1"}), events);
}

}  // namespace offline_pages
