// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/scheduled_executor.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

constexpr base::TimeDelta kDefaultDelayTimeDelta = base::Minutes(10);

}  // namespace

class ScheduledExecutorTest : public testing::Test {
 protected:
  std::shared_ptr<api::Cancelable> PostRunnableWithIdAndDelay(
      base::RunLoop& run_loop,
      const base::UnguessableToken& id,
      base::TimeDelta delay) {
    Runnable runnable = [&] {
      base::AutoLock al(id_set_lock_);
      id_set_.insert(id);

      run_loop.Quit();
    };

    std::shared_ptr<api::Cancelable> cancelable = scheduled_executor_->Schedule(
        std::move(runnable), absl::Microseconds(delay.InMicroseconds()));

    // In order to make thread-safe calls to the API of base::OneShotTimer,
    // schedule() will post a task to an internal base::SequencedTaskRunner that
    // calls Start() on a base::OneShotTimer. Executing RunUntilIdle() simply
    // ensures that the base::OneShotTimer associated with the Runnable has been
    // Start()ed, but offers no guarantee on whether the Runnable has been run()
    // or not.
    task_environment_.RunUntilIdle();

    return cancelable;
  }

  void CancelTaskAndVerifyState(std::shared_ptr<api::Cancelable> cancelable,
                                bool should_expect_success) {
    EXPECT_EQ(should_expect_success, cancelable->Cancel());

    // Ensures that the base::OneShotTimer associated with the given Cancelable
    // has been Stop()ped before this method returns.
    task_environment_.RunUntilIdle();
  }

  void VerifySetContainsId(const base::UnguessableToken& id) {
    base::AutoLock al(id_set_lock_);
    EXPECT_NE(id_set_.end(), id_set_.find(id));
  }

  size_t GetSetSize() {
    base::AutoLock al(id_set_lock_);
    return id_set_.size();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ScheduledExecutor> scheduled_executor_ =
      std::make_unique<ScheduledExecutor>(
          task_environment_.GetMainThreadTaskRunner());

 private:
  base::Lock id_set_lock_;
  std::set<base::UnguessableToken> id_set_;
};

TEST_F(ScheduledExecutorTest, SingleTaskExecutes) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id);
}

TEST_F(ScheduledExecutorTest, StaggeredTasksExecute) {
  base::RunLoop run_loop_1;
  base::UnguessableToken id_1 = base::UnguessableToken::Create();
  PostRunnableWithIdAndDelay(run_loop_1, id_1, kDefaultDelayTimeDelta);
  base::RunLoop run_loop_2;
  base::UnguessableToken id_2 = base::UnguessableToken::Create();
  PostRunnableWithIdAndDelay(run_loop_2, id_2, kDefaultDelayTimeDelta * 2);

  // Only the first scheduled task should run at first.
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id_1);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta);
  EXPECT_EQ(2u, GetSetSize());
  VerifySetContainsId(id_2);
}

TEST_F(ScheduledExecutorTest, SingleTaskCancels) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  CancelTaskAndVerifyState(cancelable, true /* should_expect_success */);
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorTest, FirstTaskCancelsAndSecondTaskExecutes) {
  base::RunLoop run_loop_1;
  base::UnguessableToken id_1 = base::UnguessableToken::Create();
  auto cancelable_1 =
      PostRunnableWithIdAndDelay(run_loop_1, id_1, kDefaultDelayTimeDelta * 2);

  base::RunLoop run_loop_2;
  base::UnguessableToken id_2 = base::UnguessableToken::Create();
  PostRunnableWithIdAndDelay(run_loop_2, id_2, kDefaultDelayTimeDelta * 3);

  CancelTaskAndVerifyState(cancelable_1, true /* should_expect_success */);
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id_2);
}

TEST_F(ScheduledExecutorTest, FailToCancelAfterRun) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorTest, FailToRunAfterCancel) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  CancelTaskAndVerifyState(cancelable, true /* should_expect_success */);
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorTest, FailToCancelAfterCancel) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  // The first call should successfully cancel the task. Subsequent invocations
  // will return false by default, as CancelableTask uses a base::OnceClosure
  // that will be consumed after the first call to cancel().
  CancelTaskAndVerifyState(cancelable, true /* should_expect_success */);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorTest, FailToCancelAfterExecutorIsDestroyed) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);
  scheduled_executor_.reset();

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorTest, FailToScheduleAfterShutdown) {
  scheduled_executor_->Shutdown();
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorTest, FailToCancelAfterShutdown) {
  scheduled_executor_->Shutdown();
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorTest, ShutdownAllowsExistingTaskToComplete) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);
  scheduled_executor_->Shutdown();

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id);
}

TEST_F(ScheduledExecutorTest, DestroyAllowExistingTaskToCompleteImmediately) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);
  scheduled_executor_.reset();

  run_loop.Run();
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id);
}

}  // namespace nearby::chrome
