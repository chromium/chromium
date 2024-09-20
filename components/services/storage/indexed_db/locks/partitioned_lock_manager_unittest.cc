// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db {
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

  BarrierBuilder(const BarrierBuilder&) = delete;
  BarrierBuilder& operator=(const BarrierBuilder&) = delete;

  base::OnceClosure AddClosure() {
    return base::BindOnce([](scoped_refptr<ContinuationRef>) {}, continuation_);
  }

 private:
  const scoped_refptr<ContinuationRef> continuation_;
};

template <typename T>
void SetValue(T* out, T value) {
  *out = value;
}

std::string IntegerKey(size_t num) {
  return base::StringPrintf("%010zd", num);
}

class PartitionedLockManagerTest : public testing::Test {
 public:
  PartitionedLockManagerTest() = default;
  ~PartitionedLockManagerTest() override = default;

 private:
  base::test::TaskEnvironment task_env_;
};

TEST_F(PartitionedLockManagerTest, TestIdPopulation) {
  PartitionedLockId lock_id = {1, "2"};
  EXPECT_EQ(1, lock_id.partition);
  EXPECT_EQ("2", lock_id.key);
}

TEST_F(PartitionedLockManagerTest, BasicAcquisition) {
  const size_t kTotalLocks = 10;
  PartitionedLockManager lock_manager;

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  base::RunLoop loop;
  PartitionedLockHolder holder1;
  PartitionedLockHolder holder2;
  {
    BarrierBuilder barrier(loop.QuitClosure());

    std::vector<PartitionedLockManager::PartitionedLockRequest> locks1_requests;
    for (size_t i = 0; i < kTotalLocks / 2; ++i) {
      PartitionedLockId lock_id = {0, IntegerKey(i)};
      locks1_requests.emplace_back(
          std::move(lock_id), PartitionedLockManager::LockType::kExclusive);
    }
    lock_manager.AcquireLocks(std::move(locks1_requests), holder1,
                              barrier.AddClosure());

    // Now acquire kTotalLocks/2 locks starting at (kTotalLocks-1) to verify
    // they acquire in the correct order.
    std::vector<PartitionedLockManager::PartitionedLockRequest> locks2_requests;
    for (size_t i = kTotalLocks - 1; i >= kTotalLocks / 2; --i) {
      PartitionedLockId lock_id = {0, IntegerKey(i)};
      locks2_requests.emplace_back(
          std::move(lock_id), PartitionedLockManager::LockType::kExclusive);
    }
    lock_manager.AcquireLocks(std::move(locks2_requests), holder2,
                              barrier.AddClosure());
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

TEST_F(PartitionedLockManagerTest, Shared) {
  PartitionedLockManager lock_manager;
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  PartitionedLockId lock_id = {0, IntegerKey(0)};

  PartitionedLockHolder locks_holder1;
  PartitionedLockHolder locks_holder2;
  base::RunLoop loop;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    EXPECT_EQ(PartitionedLockManager::TestLockResult::kFree,
              lock_manager.TestLock(
                  {lock_id, PartitionedLockManager::LockType::kShared}));
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kShared}}, locks_holder1,
        barrier.AddClosure());
    EXPECT_EQ(PartitionedLockManager::TestLockResult::kFree,
              lock_manager.TestLock(
                  {lock_id, PartitionedLockManager::LockType::kShared}));
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kShared}}, locks_holder2,
        barrier.AddClosure());
  }
  loop.Run();
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());

  EXPECT_TRUE(locks_holder1.locks.begin()->is_locked());
  EXPECT_TRUE(locks_holder2.locks.begin()->is_locked());
}

TEST_F(PartitionedLockManagerTest, SharedAndExclusiveQueuing) {
  PartitionedLockManager lock_manager;
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  PartitionedLockId lock_id = {0, IntegerKey(0)};

  PartitionedLockHolder shared_lock1_holder;
  PartitionedLockHolder shared_lock2_holder;
  PartitionedLockHolder exclusive_lock3_holder;
  PartitionedLockHolder shared_lock3_holder;

  {
    base::RunLoop loop;
    {
      BarrierBuilder barrier(loop.QuitClosure());
      lock_manager.AcquireLocks(
          {{lock_id, PartitionedLockManager::LockType::kShared}},
          shared_lock1_holder, barrier.AddClosure());
      lock_manager.AcquireLocks(
          {{lock_id, PartitionedLockManager::LockType::kShared}},
          shared_lock2_holder, barrier.AddClosure());
    }
    loop.Run();
  }
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  // Exclusive request is blocked, shared is free.
  EXPECT_EQ(PartitionedLockManager::TestLockResult::kLocked,
            lock_manager.TestLock(
                {lock_id, PartitionedLockManager::LockType::kExclusive}));
  EXPECT_EQ(PartitionedLockManager::TestLockResult::kFree,
            lock_manager.TestLock(
                {lock_id, PartitionedLockManager::LockType::kShared}));

  // Both of the following locks should be queued - the exclusive is next in
  // line, then the shared lock will come after it.
  lock_manager.AcquireLocks(
      {{lock_id, PartitionedLockManager::LockType::kExclusive}},
      exclusive_lock3_holder, base::DoNothing());
  lock_manager.AcquireLocks(
      {{lock_id, PartitionedLockManager::LockType::kShared}},
      shared_lock3_holder, base::DoNothing());
  // Flush the task queue.
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  EXPECT_FALSE(exclusive_lock3_holder.locks.empty());
  EXPECT_TRUE(shared_lock3_holder.locks.empty());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(1ll, lock_manager.RequestsWaitingForTesting());

  // Both exclusive and shared requests are blocked.
  EXPECT_EQ(PartitionedLockManager::TestLockResult::kLocked,
            lock_manager.TestLock(
                {lock_id, PartitionedLockManager::LockType::kExclusive}));
  EXPECT_EQ(PartitionedLockManager::TestLockResult::kLocked,
            lock_manager.TestLock(
                {lock_id, PartitionedLockManager::LockType::kShared}));

  exclusive_lock3_holder.locks.clear();

  // Flush the task queue to propagate the lock releases and grant the exclusive
  // lock.
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  EXPECT_FALSE(shared_lock3_holder.locks.empty());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
}

TEST_F(PartitionedLockManagerTest, PartitionsOperateSeparately) {
  PartitionedLockManager lock_manager;
  base::RunLoop loop;
  PartitionedLockHolder p0_lock_holder;
  PartitionedLockHolder p1_lock_holder;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    PartitionedLockId lock_id_p0 = {0, IntegerKey(0)};
    PartitionedLockId lock_id_p1 = {1, IntegerKey(0)};
    EXPECT_EQ(PartitionedLockManager::TestLockResult::kFree,
              lock_manager.TestLock(
                  {lock_id_p0, PartitionedLockManager::LockType::kExclusive}));
    lock_manager.AcquireLocks(
        {{lock_id_p0, PartitionedLockManager::LockType::kExclusive}},
        p0_lock_holder, barrier.AddClosure());
    EXPECT_EQ(PartitionedLockManager::TestLockResult::kFree,
              lock_manager.TestLock(
                  {lock_id_p1, PartitionedLockManager::LockType::kExclusive}));
    lock_manager.AcquireLocks(
        {{lock_id_p1, PartitionedLockManager::LockType::kExclusive}},
        p1_lock_holder, barrier.AddClosure());
  }
  loop.Run();
  EXPECT_FALSE(p0_lock_holder.locks.empty());
  EXPECT_FALSE(p1_lock_holder.locks.empty());
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  p0_lock_holder.locks.clear();
  p1_lock_holder.locks.clear();
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
}

TEST_F(PartitionedLockManagerTest, Prioritize) {
  PartitionedLockManager lock_manager;
  const PartitionedLockManager::PartitionedLockRequest common_lock_request{
      {0, IntegerKey(0)}, PartitionedLockManager::LockType::kExclusive};
  const PartitionedLockManager::PartitionedLockRequest unique_lock_request{
      {1, IntegerKey(0)}, PartitionedLockManager::LockType::kExclusive};

  // Grab the contentious lock.
  PartitionedLockHolder lock_holder;
  lock_manager.AcquireLocks({common_lock_request}, lock_holder,
                            base::DoNothing());
  EXPECT_FALSE(lock_holder.locks.empty());

  // Enqueue a request that won't be satisfied right away. This request includes
  // a unique lock request to verify that the lock manager doesn't overzealously
  // grant that lock and then hold up later (but higher priority) acquisitions.
  PartitionedLockHolder holder2, holder3;
  lock_manager.AcquireLocks({common_lock_request, unique_lock_request}, holder2,
                            base::DoNothing());
  EXPECT_TRUE(holder2.locks.empty());

  // Enqueue a request that has the highest priority.
  lock_manager.AcquireLocks(
      {common_lock_request}, holder3, base::DoNothing(),
      base::BindRepeating(
          [](const PartitionedLockHolder& other) { return true; }));
  EXPECT_TRUE(holder3.locks.empty());

  // After releasing the held lock, the higher priority request gets served.
  EXPECT_FALSE(lock_holder.locks.empty());
  lock_holder.locks.clear();
  EXPECT_TRUE(holder2.locks.empty());
  EXPECT_FALSE(holder3.locks.empty());
}

TEST_F(PartitionedLockManagerTest, NotReentrant) {
  PartitionedLockManager lock_manager;
  PartitionedLockHolder lock_holder;
  PartitionedLockId lock_id = {0, IntegerKey(0)};

  PartitionedLockManager::PartitionedLockRequest lock_request{
      lock_id, PartitionedLockManager::LockType::kExclusive};

  EXPECT_EQ(PartitionedLockManager::TestLockResult::kFree,
            lock_manager.TestLock(lock_request));

  // Grabbing a lock that's free is synchronous.
  {
    base::RunLoop run_loop;
    lock_manager.AcquireLocks({lock_request}, lock_holder,
                              run_loop.QuitClosure());
    EXPECT_FALSE(lock_holder.locks.empty());
    EXPECT_TRUE(run_loop.AnyQuitCalled());
  }

  // Enqueue two requests. The first one immediately releases the locks when
  // it's notified that they're granted.
  PartitionedLockHolder holder2, holder3;
  base::RunLoop run_loop;
  lock_manager.AcquireLocks(
      {lock_request}, holder2,
      base::BindLambdaForTesting([&holder2]() { holder2.locks.clear(); }));
  lock_manager.AcquireLocks({lock_request}, holder3, run_loop.QuitClosure());
  // Both requests are still waiting.
  EXPECT_TRUE(holder2.locks.empty());
  EXPECT_TRUE(holder3.locks.empty());

  // Release the lock. The third request will then get the locks, but only
  // asynchronously.
  lock_holder.locks.clear();
  EXPECT_FALSE(holder2.locks.empty());
  EXPECT_TRUE(holder3.locks.empty());
  run_loop.Run();
  EXPECT_TRUE(holder2.locks.empty());
  EXPECT_FALSE(holder3.locks.empty());
}

TEST_F(PartitionedLockManagerTest, LockReleased) {
  PartitionedLockManager lock_manager;
  const PartitionedLockManager::PartitionedLockRequest exclusive_lock_0{
      {0, IntegerKey(0)}, PartitionedLockManager::LockType::kExclusive};
  const PartitionedLockManager::PartitionedLockRequest exclusive_lock_1{
      {1, IntegerKey(0)}, PartitionedLockManager::LockType::kExclusive};
  const PartitionedLockManager::PartitionedLockRequest shared_lock_1{
      {1, IntegerKey(0)}, PartitionedLockManager::LockType::kShared};

  // Grant locks to a couple holders.
  PartitionedLockHolder holder1;
  lock_manager.AcquireLocks({exclusive_lock_0}, holder1, base::DoNothing());
  EXPECT_FALSE(holder1.locks.empty());

  PartitionedLockHolder holder2;
  lock_manager.AcquireLocks({exclusive_lock_1}, holder2, base::DoNothing());
  EXPECT_FALSE(holder2.locks.empty());

  // This one is blocked on both the first couple of holders.
  PartitionedLockHolder holder3;
  lock_manager.AcquireLocks({exclusive_lock_0, shared_lock_1}, holder3,
                            base::DoNothing());
  EXPECT_TRUE(holder3.locks.empty());

  // This one is only blocked on one of the initial holders.
  PartitionedLockHolder holder4, holder5;
  lock_manager.AcquireLocks({shared_lock_1}, holder4, base::DoNothing());
  lock_manager.AcquireLocks({shared_lock_1}, holder5, base::DoNothing());
  EXPECT_TRUE(holder4.locks.empty());
  EXPECT_TRUE(holder5.locks.empty());

  // Now release one of the held locks. holder3 is next in line, but it still
  // can't get all its desired locks. The next two after it, however, can be
  // granted their locks because they only need the one that was freed (and they
  // don't need exclusive access, and they aren't blocked on holder3).
  holder2.locks.clear();
  EXPECT_TRUE(holder3.locks.empty());
  EXPECT_FALSE(holder4.locks.empty());
  EXPECT_FALSE(holder5.locks.empty());
}

}  // namespace
}  // namespace content::indexed_db
