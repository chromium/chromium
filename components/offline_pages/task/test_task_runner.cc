// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/test_task_runner.h"

#include "base/bind.h"
#include "components/offline_pages/task/task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

TestTaskRunner::TestTaskRunner(
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner)
    : task_runner_(task_runner) {}

TestTaskRunner::~TestTaskRunner() {}

void TestTaskRunner::RunTask(std::unique_ptr<Task> task) {
  RunTask(task.get());
}

void TestTaskRunner::RunTask(Task* task) {
  DCHECK(task);
  Task* completed_task = nullptr;
  task->SetTaskCompletionCallbackForTesting(base::BindOnce(
      &TestTaskRunner::TaskComplete, base::Unretained(this), &completed_task));
  task->Run();
  task_runner_->RunUntilIdle();
  EXPECT_EQ(task, completed_task) << "Task did not complete";
}

void TestTaskRunner::TaskComplete(Task** completed_task_ptr, Task* task) {
  auto set_task_callback = [](Task** t_ptr, Task* t) { *t_ptr = t; };
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(set_task_callback, completed_task_ptr, task));
}

}  // namespace offline_pages
