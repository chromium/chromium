// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/disjoint_range_lock_manager.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/indexed_db/scopes/scope_lock.h"
#include "components/services/storage/indexed_db/scopes/scope_lock_range.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class BarrierBuilder {
 public:
  class ContinuationRef : public base::RefCountedThreadSafe<ContinuationRef> {
   public:
    explicit ContinuationRef(base::OnceClosure continuation)
        : continuation_(std::move(continuation)) {}

   private:
    friend class base::RefCountedThreadSafe<ContinuationRef>;
    ~ContinuationRef() = default;

    base::ScopedClosureRunner continuation_;
  };

  explicit BarrierBuilder(base::OnceClosure continuation)
      : continuation_(
            base::MakeRefCounted<ContinuationRef>(std::move(continuation))) {}

  base::OnceClosure AddClosure() {
    return base::BindOnce([](scoped_refptr<ContinuationRef>) {}, continuation_);
  }

 private:
  const scoped_refptr<ContinuationRef> continuation_;

  DISALLOW_COPY_AND_ASSIGN(BarrierBuilder);
};

template <typename T>
void SetValue(T* out, T value) {
  *out = value;
}

std::string IntegerKey(size_t num) {
  return base::StringPrintf("%010zd", num);
}

class DisjointRangeLockManagerTest : public testing::Test {
 public:
  DisjointRangeLockManagerTest() = default;
  ~DisjointRangeLockManagerTest() override = default;

 private:
  base::test::TaskEnvironment task_env_;
};

TEST_F(DisjointRangeLockManagerTest, BasicAcquisition) {
  const size_t kTotalLocks = 10;
  DisjointRangeLockManager lock_manager(1);

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  base::RunLoop loop;
  ScopesLocksHolder holder1;
  ScopesLocksHolder holder2;
  {
    BarrierBuilder barrier(loop.QuitClosure());

    std::vector<ScopesLockManager::ScopeLockRequest> locks1_requests;
    for (size_t i = 0; i < kTotalLocks / 2; ++i) {
      ScopeLockRange range = {IntegerKey(i), IntegerKey(i + 1)};
      locks1_requests.emplace_back(0, std::move(range),
                                   ScopesLockManager::LockType::kExclusive);
    }
    EXPECT_TRUE(lock_manager.AcquireLocks(locks1_requests, holder1.AsWeakPtr(),
                                          barrier.AddClosure()));

    // Now acquire kTotalLocks/2 locks starting at (kTotalLocks-1) to verify
    // they acquire in the correct order.
    std::vector<ScopesLockManager::ScopeLockRequest> locks2_requests;
    for (size_t i = kTotalLocks - 1; i >= kTotalLocks / 2; --i) {
      ScopeLockRange range = {IntegerKey(i), IntegerKey(i + 1)};
      locks2_requests.emplace_back(0, std::move(range),
                                   ScopesLockManager::LockType::kExclusive);
    }
    EXPECT_TRUE(lock_manager.AcquireLocks(locks2_requests, holder2.AsWeakPtr(),
                                          barrier.AddClosure()));
  }
  loop.Run();
  EXPECT_EQ(static_cast<int64_t>(kTotalLocks),
            lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  // All locks should be acquired.
  for (const auto& lock : holder1.locks) {
    EXPECT_TRUE(lock.is_locked());
  }
  for (const auto& lock : holder2.locks) {
    EXPECT_TRUE(lock.is_locked());
  }

  // Release locks manually
  for (auto& lock : holder1.locks) {
    lock.Release();
    EXPECT_FALSE(lock.is_locked());
  }
  for (auto& lock : holder2.locks) {
    lock.Release();
    EXPECT_FALSE(lock.is_locked());
  }

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  holder1.locks.clear();
  holder2.locks.clear();
}

TEST_F(DisjointRangeLockManagerTest, Shared) {
  DisjointRangeLockManager lock_manager(1);
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  ScopeLockRange range = {IntegerKey(0), IntegerKey(1)};

  ScopesLocksHolder locks_holder1;
  ScopesLocksHolder locks_holder2;
  base::RunLoop loop;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{0, range, ScopesLockManager::LockType::kShared}},
        locks_holder1.AsWeakPtr(), barrier.AddClosure()));
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{0, range, ScopesLockManager::LockType::kShared}},
        locks_holder2.AsWeakPtr(), barrier.AddClosure()));
  }
  loop.Run();
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());

  EXPECT_TRUE(locks_holder1.locks.begin()->is_locked());
  EXPECT_TRUE(locks_holder2.locks.begin()->is_locked());
}

TEST_F(DisjointRangeLockManagerTest, SharedAndExclusiveQueuing) {
  DisjointRangeLockManager lock_manager(1);
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  ScopeLockRange range = {IntegerKey(0), IntegerKey(1)};

  ScopesLocksHolder shared_lock1_holder;
  ScopesLocksHolder shared_lock2_holder;
  ScopesLocksHolder exclusive_lock3_holder;
  ScopesLocksHolder shared_lock3_holder;

  {
    base::RunLoop loop;
    {
      BarrierBuilder barrier(loop.QuitClosure());
      EXPECT_TRUE(lock_manager.AcquireLocks(
          {{0, range, ScopesLockManager::LockType::kShared}},
          shared_lock1_holder.AsWeakPtr(), barrier.AddClosure()));
      EXPECT_TRUE(lock_manager.AcquireLocks(
          {{0, range, ScopesLockManager::LockType::kShared}},
          shared_lock2_holder.AsWeakPtr(), barrier.AddClosure()));
    }
    loop.Run();
  }
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  // Both of the following locks should be queued - the exclusive is next in
  // line, then the shared lock will come after it.
  EXPECT_TRUE(lock_manager.AcquireLocks(
      {{0, range, ScopesLockManager::LockType::kExclusive}},
      exclusive_lock3_holder.AsWeakPtr(), base::DoNothing::Once()));
  EXPECT_TRUE(lock_manager.AcquireLocks(
      {{0, range, ScopesLockManager::LockType::kShared}},
      shared_lock3_holder.AsWeakPtr(), base::DoNothing::Once()));
  // Flush the task queue.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(exclusive_lock3_holder.locks.empty());
  EXPECT_TRUE(shared_lock3_holder.locks.empty());
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(2ll, lock_manager.RequestsWaitingForTesting());

  // Release the shared locks.
  shared_lock1_holder.locks.clear();
  shared_lock2_holder.locks.clear();

  // Flush the task queue to propagate the lock releases and grant the exclusive
  // lock.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_FALSE(exclusive_lock3_holder.locks.empty());
  EXPECT_TRUE(shared_lock3_holder.locks.empty());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(1ll, lock_manager.RequestsWaitingForTesting());

  exclusive_lock3_holder.locks.clear();

  // Flush the task queue to propagate the lock releases and grant the exclusive
  // lock.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_FALSE(shared_lock3_holder.locks.empty());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
}

TEST_F(DisjointRangeLockManagerTest, LevelsOperateSeparately) {
  DisjointRangeLockManager lock_manager(2);
  base::RunLoop loop;
  ScopesLocksHolder l0_lock_holder;
  ScopesLocksHolder l1_lock_holder;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    ScopeLockRange range = {IntegerKey(0), IntegerKey(1)};
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{0, range, ScopesLockManager::LockType::kExclusive}},
        l0_lock_holder.AsWeakPtr(), barrier.AddClosure()));
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{1, range, ScopesLockManager::LockType::kExclusive}},
        l1_lock_holder.AsWeakPtr(), barrier.AddClosure()));
  }
  loop.Run();
  EXPECT_FALSE(l0_lock_holder.locks.empty());
  EXPECT_FALSE(l1_lock_holder.locks.empty());
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  l0_lock_holder.locks.clear();
  l1_lock_holder.locks.clear();
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
}

TEST_F(DisjointRangeLockManagerTest, InvalidRequests) {
  DisjointRangeLockManager lock_manager(2);
  ScopesLocksHolder locks_holder;
  ScopeLockRange range1 = {IntegerKey(0), IntegerKey(2)};
  ScopeLockRange range2 = {IntegerKey(1), IntegerKey(3)};

  // Invalid because the ranges intersect.
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{0, range1, ScopesLockManager::LockType::kShared},
       {0, range2, ScopesLockManager::LockType::kShared}},
      locks_holder.AsWeakPtr(), base::DoNothing::Once()));
  EXPECT_TRUE(locks_holder.locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  // Invalid level.
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{-1, range1, ScopesLockManager::LockType::kShared}},
      locks_holder.AsWeakPtr(), base::DoNothing::Once()));
  EXPECT_TRUE(locks_holder.locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  // Invalid level.
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{4, range1, ScopesLockManager::LockType::kShared}},
      locks_holder.AsWeakPtr(), base::DoNothing::Once()));
  EXPECT_TRUE(locks_holder.locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  // Invalid range.
  ScopeLockRange range3 = {IntegerKey(2), IntegerKey(1)};
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{0, range3, ScopesLockManager::LockType::kShared}},
      locks_holder.AsWeakPtr(), base::DoNothing::Once()));
  EXPECT_TRUE(locks_holder.locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
}

}  // namespace
}  // namespace content
