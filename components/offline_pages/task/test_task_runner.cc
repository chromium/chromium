// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/test_task_runner.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "components/offline_pages/task/task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

void TestTaskRunner::RunTask(std::unique_ptr<Task> task) {
  TestTaskRunner::RunTask(task.get());
}

void TestTaskRunner::RunTask(Task* task) {
  DCHECK(task);
  base::RunLoop run_loop;
  task->Execute(base::BindOnce(
      [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop));
  run_loop.Run();
}

}  // namespace offline_pages
