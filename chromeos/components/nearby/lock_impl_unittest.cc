// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/lock_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

class LockImplTest : public testing::Test {
 protected:
  LockImplTest()
      : lock_(std::make_unique<LockImpl>()),
        different_thread_task_runner_(base::CreateSingleThreadTaskRunner(
            {base::ThreadPool(), base::MayBlock()})) {}

  // testing::Test
  void SetUp() override {
    // |different_thread_task_runner_| is expected to run on a different thread
    // than the main test thread.
    EXPECT_FALSE(different_thread_task_runner_->BelongsToCurrentThread());
  }

  void TearDown() override {
    // Releases the test thread's ownership of |lock_|.
    int times_to_unlock;
    {
      base::AutoLock al(lock_->bookkeeping_lock_);
      times_to_unlock = lock_->num_acquisitions_;
    }
    for (int i = 0; i < times_to_unlock; ++i)
      lock_->unlock();

    // Makes sure that outstanding LockAndUnlockFromDifferentThread() tasks in
    // |different_thread_task_runner_| finish running after the test thread
    // relinquishes its ownership of |lock_|.
    task_environment_.RunUntilIdle();

    base::AutoLock al(lock_->bookkeeping_lock_);
    EXPECT_EQ(0u, lock_->num_acquisitions_);
    EXPECT_EQ(base::kInvalidThreadId, lock_->owning_thread_id_);
  }

  void PostLockAndUnlockFromDifferentThread(
      const base::UnguessableToken& attempt_id) {
    different_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LockImplTest::LockAndUnlockFromDifferentThread,
                       base::Unretained(this), attempt_id));
  }

  // Invoked whenever attempting to verify that a parallel task has indeed
  // blocked, since there's no way to deterministically find out if that task
  // will ever unblock.
  void TinyTimeout() {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  bool HasSuccessfullyLockedWithAttemptId(
      const base::UnguessableToken& attempt_id) {
    lock_->lock();
    bool contains_key = base::Contains(successful_lock_attempts_, attempt_id);
    lock_->unlock();
    return contains_key;
  }

  location::nearby::Lock* lock() { return lock_.get(); }

  base::test::TaskEnvironment task_environment_;

 private:
  // Only meant to be posted via PostLockAndUnlockFromDifferentThread() on
  // |different_thread_task_runner_|.
  //
  // This method will only insert |attempt_id| into |successful_lock_attempts_|
  // if it succeeds in acquiring |lock_|. It will also immediately unlock()
  // after doing so because unlock() may only be called from the same thread
  // that originally called lock().
  void LockAndUnlockFromDifferentThread(
      const base::UnguessableToken& attempt_id) {
    lock_->lock();
    successful_lock_attempts_.insert(attempt_id);
    lock_->unlock();
  }

  std::unique_ptr<LockImpl> lock_;
  scoped_refptr<base::SingleThreadTaskRunner> different_thread_task_runner_;
  base::flat_set<base::UnguessableToken> successful_lock_attempts_;

  DISALLOW_COPY_AND_ASSIGN(LockImplTest);
};

TEST_F(LockImplTest, LockOnce_UnlockOnce) {
  lock()->lock();
  lock()->unlock();
}

TEST_F(LockImplTest, LockThrice_UnlockThrice) {
  lock()->lock();
  lock()->lock();
  lock()->lock();
  lock()->unlock();
  lock()->unlock();
  lock()->unlock();
}

TEST_F(LockImplTest,
       LockOnce_DisallowRelockingFromDifferentThreadUntilCurrentThreadUnlocks) {
  // Lock on current thread.
  lock()->lock();

  // Try to lock again, but on different thread.
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostLockAndUnlockFromDifferentThread(attempt_id);

  // Wait for a little, then check to see if the lock attempt failed.
  TinyTimeout();
  EXPECT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id));

  // Outstanding lock attempt succeed after unlocking from current thread.
  lock()->unlock();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id));
}

TEST_F(
    LockImplTest,
    LockThrice_DisallowRelockingFromDifferentThreadUntilCurrentThreadUnlocks) {
  // Lock on current thread.
  lock()->lock();
  lock()->lock();
  lock()->lock();

  // Try to lock again, but on different thread.
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostLockAndUnlockFromDifferentThread(attempt_id);

  // Wait for a little, then check to see if the lock attempt failed.
  TinyTimeout();
  EXPECT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id));

  // Outstanding lock attempt succeed after unlocking from current thread.
  lock()->unlock();
  lock()->unlock();
  lock()->unlock();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id));
}

TEST_F(LockImplTest, InterweavedLocking) {
  base::UnguessableToken attempt_id1 = base::UnguessableToken::Create();
  base::UnguessableToken attempt_id2 = base::UnguessableToken::Create();
  base::UnguessableToken attempt_id3 = base::UnguessableToken::Create();

  lock()->lock();
  PostLockAndUnlockFromDifferentThread(attempt_id1);
  lock()->lock();
  PostLockAndUnlockFromDifferentThread(attempt_id2);
  lock()->lock();
  PostLockAndUnlockFromDifferentThread(attempt_id3);

  TinyTimeout();
  EXPECT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id1));
  EXPECT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id2));
  EXPECT_FALSE(HasSuccessfullyLockedWithAttemptId(attempt_id3));

  lock()->unlock();
  lock()->unlock();
  lock()->unlock();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id1));
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id2));
  EXPECT_TRUE(HasSuccessfullyLockedWithAttemptId(attempt_id3));
}

TEST_F(LockImplTest, CannotUnlockBeforeAnyLocks) {
  EXPECT_DCHECK_DEATH(lock()->unlock());
}

}  // namespace nearby

}  // namespace chromeos
