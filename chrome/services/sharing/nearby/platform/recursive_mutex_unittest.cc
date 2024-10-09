// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/recursive_mutex.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class RecursiveMutexTest : public testing::Test {
 protected:
  void PostLockAndUnlockFromDifferentThread(
      base::RunLoop& run_loop,
      const base::UnguessableToken& attempt_id) {
    base::RunLoop wait_run_loop;
    auto callback = base::BindLambdaForTesting([&]() {
      wait_run_loop.Quit();

      mutex_.Lock();
      {
        // insert |attempt_id| into |successful_mutex_attempts_|
        // if it succeeds in acquiring |mutex_|, immediately Unlock()
        // after doing so because Unlock() may only be called from the same
        // thread that originally called mutex().
        base::AutoLock al(lock_);
        successful_mutex_attempts_.insert(attempt_id);
      }
      mutex_.Unlock();

      run_loop.Quit();
    });
    task_runner_->PostTask(FROM_HERE, std::move(callback));

    // Wait until callback has started.
    wait_run_loop.Run();
  }

  bool HasSuccessfullyLockedWithAttemptId(
      const base::UnguessableToken& attempt_id) {
    base::AutoLock al(lock_);
    return base::Contains(successful_mutex_attempts_, attempt_id);
  }

  RecursiveMutex& mutex() { return mutex_; }

  base::test::TaskEnvironment task_environment_;

 private:
  RecursiveMutex mutex_;
  base::Lock lock_;
  scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateTaskRunner({base::MayBlock()});
  base::flat_set<base::UnguessableToken> successful_mutex_attempts_;
};

TEST_F(RecursiveMutexTest, LockOnce_UnlockOnce) {
  mutex().Lock();
  mutex().Unlock();
}

TEST_F(RecursiveMutexTest, LockThrice_UnlockThrice) {
  mutex().Lock();
  mutex().Lock();
  mutex().Lock();
  mutex().Unlock();
  mutex().Unlock();
  mutex().Unlock();
}

TEST_F(RecursiveMutexTest,
       LockOnce_DisallowRelockingFromDifferentThreadUntilCurrentThreadUnlocks) {
  // Lock on current thread.
  mutex().Lock();

  // Try to lock again, but on different thread.
  base::RunLoop run_loop;
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostLockAndUnlockFromDifferentThread(run_loop, attempt_id);
  ASSERT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id));

  // Outstanding lock attempt succeed after unlocking from current thread.
  mutex().Unlock();
  run_loop.Run();
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id));
}

TEST_F(
    RecursiveMutexTest,
    LockThrice_DisallowRelockingFromDifferentThreadUntilCurrentThreadUnlocks) {
  // Lock on current thread.
  mutex().Lock();
  mutex().Lock();
  mutex().Lock();

  // Try to lock again, but on different thread.
  base::RunLoop run_loop;
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostLockAndUnlockFromDifferentThread(run_loop, attempt_id);
  ASSERT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id));

  // Outstanding lock attempt succeed after unlocking from current thread.
  mutex().Unlock();
  mutex().Unlock();
  mutex().Unlock();
  run_loop.Run();
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id));
}

TEST_F(RecursiveMutexTest, InterweavedLocking) {
  mutex().Lock();

  base::RunLoop run_loop_1;
  base::UnguessableToken attempt_id_1 = base::UnguessableToken::Create();
  PostLockAndUnlockFromDifferentThread(run_loop_1, attempt_id_1);
  ASSERT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id_1));

  mutex().Lock();

  base::RunLoop run_loop_2;
  base::UnguessableToken attempt_id_2 = base::UnguessableToken::Create();
  PostLockAndUnlockFromDifferentThread(run_loop_2, attempt_id_2);
  ASSERT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id_2));

  mutex().Lock();

  base::RunLoop run_loop_3;
  base::UnguessableToken attempt_id_3 = base::UnguessableToken::Create();
  PostLockAndUnlockFromDifferentThread(run_loop_3, attempt_id_3);
  ASSERT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id_3));

  mutex().Unlock();
  mutex().Unlock();
  mutex().Unlock();

  run_loop_1.Run();
  run_loop_2.Run();
  run_loop_3.Run();
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id_1));
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id_2));
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id_3));
}

TEST_F(RecursiveMutexTest, CannotUnlockBeforeAnyLocks) {
  EXPECT_DCHECK_DEATH(mutex().Unlock());
}

}  // namespace nearby::chrome
