// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TASK_TEST_BASE_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TASK_TEST_BASE_H_

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/offline_pages/task/task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

// Base class for testing prefetch requests with simulated responses.
class TaskTestBase : public testing::Test {
 public:
  TaskTestBase();
  ~TaskTestBase() override;

  void SetUp() override;
  void TearDown() override;

  // Runs task with expectation that it correctly completes.
  // Task is also cleaned up after completing.
  void RunTask(std::unique_ptr<Task> task);
  // Runs task with expectation that it correctly completes.
  // Task is not cleaned up after completing.
  void RunTask(Task* task);
  void RunUntilIdle();
  void FastForwardBy(base::TimeDelta delta);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TASK_TEST_BASE_H_
