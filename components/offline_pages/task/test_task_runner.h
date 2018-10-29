// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_RUNNER_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"

namespace offline_pages {
class Task;

// Tool for running (task queue related) tasks in test.
class TestTaskRunner {
 public:
  explicit TestTaskRunner(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner);
  ~TestTaskRunner();

  // Runs task with expectation that it correctly completes.
  // Task is also cleaned up after completing.
  void RunTask(std::unique_ptr<Task> task);
  // Runs task with expectation that it correctly completes.
  // Task is not cleaned up after completing.
  void RunTask(Task* task);

 private:
  void TaskComplete(Task** completed_task_ptr, Task* task);

  // Certainly confusing, but internal task runner, is simply a test version of
  // a single thread task runner. It is used for running closures.
  // The difference between that and the encapsulating task runner, is that a
  // task in a Task Queue sense may be build of multiple closures.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestTaskRunner);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_RUNNER_H_
