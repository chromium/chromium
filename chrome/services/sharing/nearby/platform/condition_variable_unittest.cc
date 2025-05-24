// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/condition_variable.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/services/sharing/nearby/platform/mutex.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class ConditionVariableTest : public testing::Test {
 protected:
  void WaitOnConditionVariableFromParallelSequence(
      base::RunLoop& run_loop,
      const base::UnguessableToken& attempt_id) {
    base::RunLoop wait_run_loop;
    auto callback = base::BindLambdaForTesting([&]() {
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;

      wait_run_loop.Quit();

      mutex_.Lock();
      condition_variable_.Wait();
      mutex_.Unlock();

      {
        base::AutoLock al(coordination_lock_);
        successful_run_attempts_.insert(attempt_id);
      }

      run_loop.Quit();
    });
    task_runner_->PostTask(FROM_HERE, std::move(callback));

    // Wait until callback has started.
    wait_run_loop.Run();
  }

  bool HasSuccessfullyRunWithAttemptId(
      const base::UnguessableToken& attempt_id) {
    base::AutoLock al(coordination_lock_);
    return base::Contains(successful_run_attempts_, attempt_id);
  }

  void NotifyConditionVariable() {
    mutex_.Lock();
    condition_variable_.Notify();
    mutex_.Unlock();
  }

  base::test::TaskEnvironment task_environment_;

 private:
  Mutex mutex_;
  ConditionVariable condition_variable_{&mutex_};
  scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateTaskRunner({base::MayBlock()});
  base::Lock coordination_lock_;
  base::flat_set<base::UnguessableToken> successful_run_attempts_;
};

// Speculatively disabled on ChromeOS bots due to https://crbug.com/1186166
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SingleSequence_BlocksOnWaitAndUnblocksOnNotify \
  DISABLED_SingleSequence_BlocksOnWaitAndUnblocksOnNotify
#else
#define MAYBE_SingleSequence_BlocksOnWaitAndUnblocksOnNotify \
  SingleSequence_BlocksOnWaitAndUnblocksOnNotify
#endif
TEST_F(ConditionVariableTest,
       MAYBE_SingleSequence_BlocksOnWaitAndUnblocksOnNotify) {
  base::RunLoop run_loop;
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  WaitOnConditionVariableFromParallelSequence(run_loop, attempt_id);
  ASSERT_FALSE(HasSuccessfullyRunWithAttemptId(attempt_id));

  // Should unblock after notify().
  NotifyConditionVariable();

  run_loop.Run();
  EXPECT_TRUE(HasSuccessfullyRunWithAttemptId(attempt_id));
}

TEST_F(ConditionVariableTest,
       DISABLED_MultipleSequences_BlocksOnWaitAndUnblocksOnNotify) {
  base::RunLoop run_loop_1;
  base::UnguessableToken attempt_id_1 = base::UnguessableToken::Create();
  WaitOnConditionVariableFromParallelSequence(run_loop_1, attempt_id_1);
  base::RunLoop run_loop_2;
  base::UnguessableToken attempt_id_2 = base::UnguessableToken::Create();
  WaitOnConditionVariableFromParallelSequence(run_loop_2, attempt_id_2);
  base::RunLoop run_loop_3;
  base::UnguessableToken attempt_id_3 = base::UnguessableToken::Create();
  WaitOnConditionVariableFromParallelSequence(run_loop_3, attempt_id_3);

  ASSERT_FALSE(HasSuccessfullyRunWithAttemptId(attempt_id_1));
  ASSERT_FALSE(HasSuccessfullyRunWithAttemptId(attempt_id_2));
  ASSERT_FALSE(HasSuccessfullyRunWithAttemptId(attempt_id_3));

  // All should unblock after notify().
  NotifyConditionVariable();

  run_loop_1.Run();
  run_loop_2.Run();
  run_loop_3.Run();
  EXPECT_TRUE(HasSuccessfullyRunWithAttemptId(attempt_id_1));
  EXPECT_TRUE(HasSuccessfullyRunWithAttemptId(attempt_id_2));
  EXPECT_TRUE(HasSuccessfullyRunWithAttemptId(attempt_id_3));
}

}  // namespace nearby::chrome
