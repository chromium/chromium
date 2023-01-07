// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_RUNNER_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_RUNNER_H_

#include <memory>

namespace offline_pages {
class Task;

// Tool for running (task queue related) tasks in test.
class TestTaskRunner {
 public:
  // Runs task with expectation that it correctly completes.
  // Task is also cleaned up after completing.
  static void RunTask(std::unique_ptr<Task> task);
  // Runs task with expectation that it correctly completes.
  // Task is not cleaned up after completing.
  static void RunTask(Task* task);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_RUNNER_H_
