// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/submittable_executor.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

// To test Execute(), which has no return value, each task is assigned a unique
// ID. This ID is added to |executed_tasks_| when the task is Run(). Thus, the
// presence of an ID within |executed_tasks_| means the associated task has
// completed execution.
class SingleThreadExecutorTest : public testing::Test {
 protected:
  Runnable CreateTrackedRunnable(base::RunLoop& run_loop,
                                 const base::UnguessableToken& task_id) {
    return [&] {
      base::AutoLock al(executed_tasks_lock_);
      executed_tasks_.insert(task_id);
      run_loop.Quit();
    };
  }

  bool HasTaskExecuted(const base::UnguessableToken& task_id) {
    base::AutoLock al(executed_tasks_lock_);
    return executed_tasks_.find(task_id) != executed_tasks_.end();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SubmittableExecutor> single_thread_executor_ =
      std::make_unique<SubmittableExecutor>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));

 private:
  base::Lock executed_tasks_lock_;
  std::set<base::UnguessableToken> executed_tasks_;
};

TEST_F(SingleThreadExecutorTest, Submit) {
  base::RunLoop run_loop;
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  EXPECT_TRUE(single_thread_executor_->DoSubmit(
      CreateTrackedRunnable(run_loop, task_id)));

  run_loop.Run();
  EXPECT_TRUE(HasTaskExecuted(task_id));
}

TEST_F(SingleThreadExecutorTest, Execute) {
  base::RunLoop run_loop;
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  single_thread_executor_->Execute(CreateTrackedRunnable(run_loop, task_id));

  run_loop.Run();
  EXPECT_TRUE(HasTaskExecuted(task_id));
}

TEST_F(SingleThreadExecutorTest, ShutdownPreventsFurtherTasks) {
  single_thread_executor_->Shutdown();
  base::RunLoop run_loop;
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  EXPECT_FALSE(single_thread_executor_->DoSubmit(
      CreateTrackedRunnable(run_loop, task_id)));

  EXPECT_FALSE(HasTaskExecuted(task_id));
}

TEST_F(SingleThreadExecutorTest, DestroyAllowExistingTaskToComplete) {
  base::RunLoop run_loop;
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  EXPECT_TRUE(single_thread_executor_->DoSubmit(
      CreateTrackedRunnable(run_loop, task_id)));
  EXPECT_FALSE(HasTaskExecuted(task_id));
  single_thread_executor_.reset();

  run_loop.Run();
  EXPECT_TRUE(HasTaskExecuted(task_id));
}

}  // namespace nearby::chrome
