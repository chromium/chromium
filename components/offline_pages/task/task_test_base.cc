// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task_test_base.h"

#include "base/test/mock_callback.h"
#include "components/offline_pages/task/test_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace offline_pages {

TaskTestBase::TaskTestBase() = default;

TaskTestBase::~TaskTestBase() = default;

void TaskTestBase::SetUp() {
  testing::Test::SetUp();
}

void TaskTestBase::TearDown() {
  task_environment_.FastForwardUntilNoTasksRemain();
  testing::Test::TearDown();
}

void TaskTestBase::FastForwardBy(base::TimeDelta delta) {
  task_environment_.FastForwardBy(delta);
}

void TaskTestBase::RunUntilIdle() {
  task_environment_.RunUntilIdle();
}

void TaskTestBase::RunTask(std::unique_ptr<Task> task) {
  TestTaskRunner::RunTask(std::move(task));
}

void TaskTestBase::RunTask(Task* task) {
  TestTaskRunner::RunTask(task);
}

}  // namespace offline_pages
