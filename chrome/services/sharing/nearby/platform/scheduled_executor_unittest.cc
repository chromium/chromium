// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/scheduled_executor.h"

#include <atomic>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
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

    return cancelable;
  }

  void CancelTaskAndVerifyState(std::shared_ptr<api::Cancelable> cancelable,
                                bool should_expect_success) {
    EXPECT_EQ(should_expect_success, cancelable->Cancel());
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
  // will return false because the runnable has been removed.
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

TEST_F(ScheduledExecutorTest, CancelFromDifferentSequence) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  scoped_refptr<base::SequencedTaskRunner> other_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  base::RunLoop cancel_run_loop;
  other_task_runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                EXPECT_TRUE(cancelable->Cancel());
                                cancel_run_loop.Quit();
                              }));
  cancel_run_loop.Run();

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorTest, ConcurrentCancelFromMultipleThreads) {
  base::RunLoop run_loop;
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable =
      PostRunnableWithIdAndDelay(run_loop, id, kDefaultDelayTimeDelta);

  constexpr size_t kNumTasks = 4;
  std::atomic<int> success_count{0};
  base::RunLoop cancel_run_loop;
  auto barrier = base::BarrierClosure(kNumTasks, cancel_run_loop.QuitClosure());

  for (size_t i = 0; i < kNumTasks; ++i) {
    base::ThreadPool::PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                 if (cancelable->Cancel()) {
                                   ++success_count;
                                 }
                                 barrier.Run();
                               }));
  }

  cancel_run_loop.Run();

  EXPECT_EQ(1, success_count.load());
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorTest, ConcurrentCancelDuringDestruction) {
  for (int iter = 0; iter < 50; ++iter) {
    auto executor = std::make_unique<ScheduledExecutor>(
        task_environment_.GetMainThreadTaskRunner());
    base::TestWaitableEvent go;
    std::shared_ptr<api::Cancelable> cancelable =
        executor->Schedule([&go]() { go.Signal(); }, absl::Hours(1));

    base::TestWaitableEvent cancel_done;
    base::ThreadPool::PostTask(
        FROM_HERE, {base::WithBaseSyncPrimitives()},
        base::BindLambdaForTesting([&go, &cancel_done, cancelable]() {
          go.Wait();
          cancelable->Cancel();
          cancel_done.Signal();
        }));

    executor.reset();
    cancel_done.Wait();
  }
}

TEST_F(ScheduledExecutorTest, CancelWhileTaskIsRunningReturnsFalse) {
  base::TestWaitableEvent task_started;
  base::TestWaitableEvent allow_task_finish;
  std::atomic<bool> cancel_result{true};
  base::RunLoop task_run_loop;

  std::shared_ptr<api::Cancelable> cancelable = scheduled_executor_->Schedule(
      [&]() {
        task_started.Signal();
        allow_task_finish.Wait();
        task_run_loop.Quit();
      },
      absl::ZeroDuration());

  base::ThreadPool::PostTask(FROM_HERE, {base::WithBaseSyncPrimitives()},
                             base::BindLambdaForTesting([&]() {
                               task_started.Wait();
                               cancel_result.store(cancelable->Cancel(),
                                                   std::memory_order_release);
                               allow_task_finish.Signal();
                             }));

  task_run_loop.Run();

  EXPECT_FALSE(cancel_result.load());
}

TEST_F(ScheduledExecutorTest, MultiplePendingTasksWithMixedCancellation) {
  constexpr int kNumTasks = 20;
  std::vector<std::shared_ptr<api::Cancelable>> cancelables;
  std::atomic<int> run_count{0};

  for (int i = 0; i < kNumTasks; ++i) {
    cancelables.push_back(scheduled_executor_->Schedule(
        [&run_count]() { run_count.fetch_add(1, std::memory_order_relaxed); },
        absl::Milliseconds(100 * (i + 1))));
  }

  // Cancel even-indexed tasks.
  for (int i = 0; i < kNumTasks; i += 2) {
    EXPECT_TRUE(cancelables[i]->Cancel());
  }

  task_environment_.FastForwardBy(base::Milliseconds(100 * (kNumTasks + 1)));

  // Exactly half of the tasks (the odd-indexed ones) should have executed.
  EXPECT_EQ(kNumTasks / 2, run_count.load());

  // Attempting to cancel already-executed tasks should return false.
  for (int i = 1; i < kNumTasks; i += 2) {
    EXPECT_FALSE(cancelables[i]->Cancel());
  }
}

TEST_F(ScheduledExecutorTest, ConcurrentCancelOfMultipleTasks) {
  constexpr size_t kNumTasks = 100;
  std::vector<std::shared_ptr<api::Cancelable>> cancelables;
  cancelables.reserve(kNumTasks);

  for (size_t i = 0; i < kNumTasks; ++i) {
    cancelables.push_back(
        scheduled_executor_->Schedule([]() {}, absl::Minutes(10)));
  }

  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(kNumTasks, run_loop.QuitClosure());

  for (size_t i = 0; i < kNumTasks; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([cancelable = cancelables[i], barrier]() {
          cancelable->Cancel();
          barrier.Run();
        }));
  }

  run_loop.Run();
}

TEST_F(ScheduledExecutorTest, ExecuteRunsImmediately) {
  base::RunLoop run_loop;
  bool executed = false;
  scheduled_executor_->Execute([&]() {
    executed = true;
    run_loop.Quit();
  });
  run_loop.Run();
  EXPECT_TRUE(executed);
}

TEST_F(ScheduledExecutorTest, NegativeDurationRunsImmediately) {
  base::RunLoop run_loop;
  bool executed = false;
  scheduled_executor_->Schedule(
      [&]() {
        executed = true;
        run_loop.Quit();
      },
      absl::Seconds(-5));
  run_loop.Run();
  EXPECT_TRUE(executed);
}

}  // namespace nearby::chrome
