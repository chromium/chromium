// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/partitioned_lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
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

TEST_F(PartitionedLockManagerTest, AcquireOptionsEnsureAsync) {
  base::RunLoop loop;
  bool callback_ran = false;

  PartitionedLockManager lock_manager;
  PartitionedLockHolder lock_holder;
  PartitionedLockId lock_id = {0, IntegerKey(0)};

  EXPECT_EQ(PartitionedLockManager::TestLockResult::kFree,
            lock_manager.TestLock(
                {lock_id, PartitionedLockManager::LockType::kShared}));

  lock_manager.AcquireLocks(
      {{lock_id, PartitionedLockManager::LockType::kShared}}, lock_holder,
      base::BindOnce(
          [](base::RunLoop* loop, bool* callback_ran) {
            *callback_ran = true;
            loop->Quit();
          },
          base::Unretained(&loop), base::Unretained(&callback_ran)));
  EXPECT_FALSE(callback_ran);

  loop.Run();
  EXPECT_TRUE(callback_ran);
}

TEST_F(PartitionedLockManagerTest, Locations) {
  PartitionedLockManager lock_manager;

  base::Location location1 = FROM_HERE;
  base::Location location2 = FROM_HERE;
  base::Location location3 = FROM_HERE;

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  PartitionedLockId lock_id = {0, "foo"};

  PartitionedLockHolder holder1;
  PartitionedLockHolder holder2;
  PartitionedLockHolder holder3;
  {
    base::test::TestFuture<void> lock_acquired;
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kShared}}, holder1,
        lock_acquired.GetCallback(), location1);
    ASSERT_TRUE(lock_acquired.Wait());
  }
  {
    base::test::TestFuture<void> lock_acquired;
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kShared}}, holder2,
        lock_acquired.GetCallback(), location2);
    ASSERT_TRUE(lock_acquired.Wait());
  }
  {
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kExclusive}}, holder3,
        base::DoNothing(), location3);
  }
  std::vector<base::Location> held_locations =
      lock_manager.GetHeldAndQueuedLockLocations(
          {{lock_id, PartitionedLockManager::LockType::kShared}});
  ASSERT_EQ(held_locations.size(), 3ul);
  EXPECT_THAT(held_locations,
              testing::UnorderedElementsAre(location1, location2, location3));
}

TEST_F(PartitionedLockManagerTest, DebugValueNoCrash) {
  PartitionedLockManager lock_manager;

  base::Location location1 = FROM_HERE;
  base::Location location2 = FROM_HERE;
  base::Location location3 = FROM_HERE;

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  PartitionedLockId lock_id = {0, "foo"};

  PartitionedLockHolder holder1;
  PartitionedLockHolder holder2;
  PartitionedLockHolder holder3;
  {
    base::test::TestFuture<void> lock_acquired;
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kShared}}, holder1,
        lock_acquired.GetCallback(), location1);
    ASSERT_TRUE(lock_acquired.Wait());
  }
  {
    base::test::TestFuture<void> lock_acquired;
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kShared}}, holder2,
        lock_acquired.GetCallback(), location2);
    ASSERT_TRUE(lock_acquired.Wait());
  }
  {
    lock_manager.AcquireLocks(
        {{lock_id, PartitionedLockManager::LockType::kExclusive}}, holder3,
        base::DoNothing(), location3);
  }
  base::Value debug_value =
      lock_manager.ToDebugValue([](const PartitionedLockId& lock) {
        return base::StringPrintf("%i %s", lock.partition, lock.key.c_str());
      });
  EXPECT_TRUE(debug_value.is_dict());
}

TEST_F(PartitionedLockManagerTest, DeadlockPreventionWithOrdering) {
  PartitionedLockManager lock_manager;

  PartitionedLockHolder holder1;
  PartitionedLockHolder holder2;

  PartitionedLockId lock_a = {0, ""};
  PartitionedLockId lock_b = {1, ""};

  // Deadlock case that ordering prevents (where, when given multiple locks,
  // locks are only requested after the previous one is granted):
  // 1. Holder 1 requests A
  // 2. Holder 2 requests A and B
  // 3. Holder 1 requests B.
  // Deadlock unless holder 2 didn't get B due to lack of A availability.

  base::test::TestFuture<void> step_1;
  lock_manager.AcquireLocks(
      {PartitionedLockManager::PartitionedLockRequest(
          lock_a, PartitionedLockManager::LockType::kExclusive)},
      holder1, step_1.GetCallback());
  EXPECT_TRUE(step_1.Wait());

  base::test::TestFuture<void> step_2;
  lock_manager.AcquireLocks(
      {PartitionedLockManager::PartitionedLockRequest(
           lock_a, PartitionedLockManager::LockType::kExclusive),
       PartitionedLockManager::PartitionedLockRequest(
           lock_b, PartitionedLockManager::LockType::kExclusive)},
      holder2, step_2.GetCallback());

  base::test::TestFuture<void> step_3;
  lock_manager.AcquireLocks(
      {PartitionedLockManager::PartitionedLockRequest(
          lock_b, PartitionedLockManager::LockType::kExclusive)},
      holder1, step_3.GetCallback());

  // The 3rd step should complete before the 2nd step.
  EXPECT_TRUE(step_3.Wait()) << "Step two status: " << step_2.IsReady();
  EXPECT_FALSE(step_2.IsReady());

  holder1.locks.clear();

  EXPECT_TRUE(step_2.Wait());
}

}  // namespace
}  // namespace web_app
