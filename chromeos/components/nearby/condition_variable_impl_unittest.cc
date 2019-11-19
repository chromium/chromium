// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/condition_variable_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/components/nearby/lock_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

namespace {

class FakeLock : public LockBase {
 public:
  FakeLock() : condition_variable_(&lock_) {}
  ~FakeLock() override = default;

  // location::nearby::Lock:
  void lock() override {
    base::AutoLock al(lock_);
    ++num_locks_;
    condition_variable_.Signal();
  }

  void unlock() override {
    base::AutoLock al(lock_);
    ++num_unlocks_;
    condition_variable_.Signal();
  }

  // chromeos::nearby::LockBase:
  bool IsHeldByCurrentThread() override { return is_held_by_current_thread_; }

  void set_is_held_by_current_thread(bool value) {
    is_held_by_current_thread_ = value;
  }

  int num_locks() {
    base::AutoLock al(lock_);
    return num_locks_;
  }

  int num_unlocks() {
    base::AutoLock al(lock_);
    return num_unlocks_;
  }

  void WaitForLocksAndUnlocks(int expected_num_locks,
                              int expected_num_unlocks) {
    base::AutoLock al(lock_);
    while (expected_num_locks != num_locks_ ||
           expected_num_unlocks != num_unlocks_) {
      condition_variable_.Wait();
    }
  }

 private:
  base::Lock lock_;
  base::ConditionVariable condition_variable_;
  bool is_held_by_current_thread_ = false;
  int num_locks_ = 0;
  int num_unlocks_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeLock);
};

}  // namespace

class ConditionVariableImplTest : public testing::Test {
 protected:
  ConditionVariableImplTest()
      : fake_lock_(std::make_unique<FakeLock>()),
        condition_variable_(
            std::make_unique<ConditionVariableImpl>(fake_lock_.get())) {}

  // testing::Test
  void TearDown() override {
    task_environment_.RunUntilIdle();
    EXPECT_EQ(fake_lock_->num_locks(), fake_lock_->num_unlocks());
  }

  location::nearby::ConditionVariable* condition_variable() {
    return condition_variable_.get();
  }

  FakeLock* fake_lock() { return fake_lock_.get(); }

  void WaitOnConditionVariableFromParallelSequence(bool should_succeed) {
    base::PostTask(
        FROM_HERE,
        base::BindOnce(&ConditionVariableImplTest::WaitOnConditionVariable,
                       base::Unretained(this), should_succeed));
  }

  // Invoked whenever attempting to verify that a parallel task has indeed
  // blocked, since there's no way to deterministically find out if that task
  // will ever unblock.
  void TinyTimeout() {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  void VerifyNumLocksAndUnlocks(int expected_num_locks,
                                int expected_num_unlocks) {
    EXPECT_EQ(expected_num_locks, fake_lock_->num_locks());
    EXPECT_EQ(expected_num_unlocks, fake_lock_->num_unlocks());
  }

  // To ensure that |condition_variable_| is blocking as expected, wait until
  // its |fake_lock_| has been locked and unlocked once per blocked sequence.
  // Then manually wait, and verify afterwards that the number of locks and
  // unlocks remain unchanged.
  void VerifyBlockedConditionVariable(int expected_num_blocked_sequences) {
    fake_lock()->WaitForLocksAndUnlocks(
        expected_num_blocked_sequences /* expected_num_locks */,
        expected_num_blocked_sequences /* expected_num_unlocks */);
    TinyTimeout();
    VerifyNumLocksAndUnlocks(
        expected_num_blocked_sequences /* expected_num_locks */,
        expected_num_blocked_sequences /* expected_num_unlocks */);
  }

  base::test::TaskEnvironment task_environment_;

 private:
  void WaitOnConditionVariable(bool should_succeed) {
    const base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;

    // ConditionVariable::wait() should only be called by a thread that already
    // owns the associated Lock. This is not tested as the expected behavior is
    // either undefined or specified by the particular Lock implementation.
    fake_lock_->lock();

    if (should_succeed)
      condition_variable_->wait();
    else
      EXPECT_DCHECK_DEATH(condition_variable_->wait());

    // ConditionVariable::wait() will call Lock::lock() after unblocking, so
    // this undoes that.
    fake_lock_->unlock();
  }

  std::unique_ptr<FakeLock> fake_lock_;
  std::unique_ptr<location::nearby::ConditionVariable> condition_variable_;

  DISALLOW_COPY_AND_ASSIGN(ConditionVariableImplTest);
};

TEST_F(ConditionVariableImplTest,
       SingleSequence_BlocksOnWaitAndUnblocksOnNotify) {
  WaitOnConditionVariableFromParallelSequence(true /* should_succeed */);
  VerifyBlockedConditionVariable(1 /* expected_num_blocked_sequences */);

  // Should unblock after notify().
  condition_variable()->notify();
  task_environment_.RunUntilIdle();
  VerifyNumLocksAndUnlocks(2 /* expected_num_locks */,
                           2 /* expected_num_unlocks */);
}

TEST_F(ConditionVariableImplTest,
       MultipleSequences_BlocksOnWaitAndUnblocksOnNotify) {
  WaitOnConditionVariableFromParallelSequence(true /* should_succeed */);
  WaitOnConditionVariableFromParallelSequence(true /* should_succeed */);
  WaitOnConditionVariableFromParallelSequence(true /* should_succeed */);
  VerifyBlockedConditionVariable(3 /* expected_num_blocked_sequences */);

  // All should unblock after notify().
  condition_variable()->notify();
  task_environment_.RunUntilIdle();
  VerifyNumLocksAndUnlocks(6 /* expected_num_locks */,
                           6 /* expected_num_unlocks */);
}

TEST_F(ConditionVariableImplTest, ThreadCannotWaitIfStillOwnsLock) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  fake_lock()->set_is_held_by_current_thread(true);
  WaitOnConditionVariableFromParallelSequence(false /* should_succeed */);
}

}  // namespace nearby

}  // namespace chromeos
