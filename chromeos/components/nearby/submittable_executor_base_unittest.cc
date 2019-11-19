// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/submittable_executor_base.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/components/nearby/library/callable.h"
#include "chromeos/components/nearby/library/exception.h"
#include "chromeos/components/nearby/library/future.h"
#include "chromeos/components/nearby/library/multi_thread_executor.h"
#include "chromeos/components/nearby/library/runnable.h"
#include "chromeos/components/nearby/library/single_thread_executor.h"
#include "chromeos/components/nearby/multi_thread_executor_impl.h"
#include "chromeos/components/nearby/single_thread_executor_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

namespace {

class SimpleRunnable : public location::nearby::Runnable {
 public:
  explicit SimpleRunnable(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  ~SimpleRunnable() override = default;

  void run() override {
    EXPECT_FALSE(closure_.is_null());
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
};

class SimpleCallable : public location::nearby::Callable<bool> {
 public:
  SimpleCallable() = default;
  ~SimpleCallable() override = default;

  location::nearby::ExceptionOr<bool> call() override {
    return location::nearby::ExceptionOr<bool>(true);
  }
};

}  // namespace

// SubmittableExecutorBase is tested via both the SingleThreadExecutorImpl and
// MultiThreadExecutorImpl interfaces. There are two tests for each test case,
// one for each implementation.
//
// To test execute(), which has no return value, each task is assigned a unique
// ID. This ID is added to |executed_tasks_| when the task is Run(). Thus, the
// presence of an ID within |executed_tasks_| means the associated task has
// completed execution.
class SubmittableExecutorBaseTest : public testing::Test {
 protected:
  SubmittableExecutorBaseTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  ~SubmittableExecutorBaseTest() override = default;

  void InitSingleThreadExecutor() {
    single_thread_executor_ = std::make_unique<SingleThreadExecutorImpl>();
  }

  void InitMultiThreadExecutor() {
    multi_thread_executor_ = std::make_unique<MultiThreadExecutorImpl>();
  }

  location::nearby::SingleThreadExecutor<SingleThreadExecutorImpl>*
  single_thread_executor() {
    EXPECT_TRUE(single_thread_executor_);
    return single_thread_executor_.get();
  }

  location::nearby::MultiThreadExecutor<MultiThreadExecutorImpl>*
  multi_thread_executor() {
    EXPECT_TRUE(multi_thread_executor_);
    return multi_thread_executor_.get();
  }

  void ExecuteRunnableWithId(const base::UnguessableToken& task_id,
                             bool single_thread) {
    single_thread ? single_thread_executor_->execute(CreateRunnable(task_id))
                  : multi_thread_executor_->execute(CreateRunnable(task_id));

    // Ensures the Runnable will complete execution before this method returns.
    task_environment_.RunUntilIdle();
  }

  std::shared_ptr<location::nearby::Future<bool>> SubmitCallable(
      bool single_thread) {
    std::shared_ptr<location::nearby::Future<bool>> future =
        single_thread
            ? single_thread_executor_->submit(
                  static_cast<
                      std::shared_ptr<location::nearby::Callable<bool>>>(
                      std::make_shared<SimpleCallable>()))
            : multi_thread_executor_->submit(
                  static_cast<
                      std::shared_ptr<location::nearby::Callable<bool>>>(
                      std::make_shared<SimpleCallable>()));

    // Ensures the Callable will complete execution before this method returns.
    task_environment_.RunUntilIdle();

    return future;
  }

  bool HasTaskExecuted(const base::UnguessableToken& task_id) {
    base::AutoLock al(executed_tasks_lock_);
    return executed_tasks_.find(task_id) != executed_tasks_.end();
  }

  size_t GetNumExecutedTasks() {
    base::AutoLock al(executed_tasks_lock_);
    return executed_tasks_.size();
  }

  std::shared_ptr<location::nearby::Callable<bool>> CreateCallable() {
    return std::make_shared<SimpleCallable>();
  }

  std::shared_ptr<location::nearby::Runnable> CreateRunnable(
      const base::UnguessableToken& task_id) {
    return std::make_shared<SimpleRunnable>(
        base::BindRepeating(&SubmittableExecutorBaseTest::OnTaskRun,
                            base::Unretained(this), task_id));
  }

  base::test::TaskEnvironment task_environment_;

 private:
  void OnTaskRun(const base::UnguessableToken& task_id) {
    base::AutoLock al(executed_tasks_lock_);
    executed_tasks_.insert(task_id);
  }

  std::unique_ptr<
      location::nearby::SingleThreadExecutor<SingleThreadExecutorImpl>>
      single_thread_executor_;
  std::unique_ptr<
      location::nearby::MultiThreadExecutor<MultiThreadExecutorImpl>>
      multi_thread_executor_;
  base::Lock executed_tasks_lock_;
  std::set<base::UnguessableToken> executed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(SubmittableExecutorBaseTest);
};

TEST_F(SubmittableExecutorBaseTest, SingleThread_Submit) {
  InitSingleThreadExecutor();
  std::shared_ptr<location::nearby::Future<bool>> future =
      SubmitCallable(true /* single_thread */);

  EXPECT_TRUE(future->get().ok());
  EXPECT_TRUE(future->get().result());
}

TEST_F(SubmittableExecutorBaseTest, MultiThread_Submit) {
  InitMultiThreadExecutor();
  std::shared_ptr<location::nearby::Future<bool>> future =
      SubmitCallable(false /* single_thread */);

  EXPECT_TRUE(future->get().ok());
  EXPECT_TRUE(future->get().result());
}

TEST_F(SubmittableExecutorBaseTest, SingleThread_Execute) {
  InitSingleThreadExecutor();
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  ExecuteRunnableWithId(task_id, true /* single_thread */);

  EXPECT_EQ(1u, GetNumExecutedTasks());
  EXPECT_TRUE(HasTaskExecuted(task_id));
}

TEST_F(SubmittableExecutorBaseTest, MultiThread_Execute) {
  InitMultiThreadExecutor();
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  ExecuteRunnableWithId(task_id, false /* single_thread */);

  EXPECT_EQ(1u, GetNumExecutedTasks());
  EXPECT_TRUE(HasTaskExecuted(task_id));
}

TEST_F(SubmittableExecutorBaseTest, SingleThread_ShutdownPreventsFurtherTasks) {
  InitSingleThreadExecutor();
  single_thread_executor()->shutdown();
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  ExecuteRunnableWithId(task_id, true /* single_thread */);
  std::shared_ptr<location::nearby::Future<bool>> future =
      SubmitCallable(true /*single_thread */);

  EXPECT_EQ(0u, GetNumExecutedTasks());
  EXPECT_FALSE(future->get().ok());
  EXPECT_EQ(location::nearby::Exception::INTERRUPTED,
            future->get().exception());
}

TEST_F(SubmittableExecutorBaseTest, MultiThread_ShutdownPreventsFurtherTasks) {
  InitMultiThreadExecutor();
  multi_thread_executor()->shutdown();
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  ExecuteRunnableWithId(task_id, false /* single_thread */);
  std::shared_ptr<location::nearby::Future<bool>> future =
      SubmitCallable(false /* single_thread */);

  EXPECT_EQ(0u, GetNumExecutedTasks());
  EXPECT_FALSE(future->get().ok());
  EXPECT_EQ(location::nearby::Exception::INTERRUPTED,
            future->get().exception());
}

TEST_F(SubmittableExecutorBaseTest,
       SingleThread_ShutdownAllowsExistingTaskToComplete) {
  InitSingleThreadExecutor();
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  single_thread_executor()->execute(CreateRunnable(task_id));
  single_thread_executor()->shutdown();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, GetNumExecutedTasks());
  EXPECT_TRUE(HasTaskExecuted(task_id));
}

TEST_F(SubmittableExecutorBaseTest,
       MultiThread_ShutdownAllowsExistingTaskToComplete) {
  InitMultiThreadExecutor();
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  multi_thread_executor()->execute(CreateRunnable(task_id));
  multi_thread_executor()->shutdown();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, GetNumExecutedTasks());
  EXPECT_TRUE(HasTaskExecuted(task_id));
}

}  // namespace nearby

}  // namespace chromeos
