// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

const int kInvalidUniqueID = -1;

class TestDiscardableSharedMemory : public base::DiscardableSharedMemory {
 public:
  TestDiscardableSharedMemory() {}

  explicit TestDiscardableSharedMemory(base::UnsafeSharedMemoryRegion region)
      : DiscardableSharedMemory(std::move(region)) {}

  void SetNow(base::Time now) { now_ = now; }

 private:
  // Overriden from base::DiscardableSharedMemory:
  base::Time Now() const override { return now_; }

  base::Time now_;
};

class TestDiscardableSharedMemoryManager
    : public DiscardableSharedMemoryManager {
 public:
  TestDiscardableSharedMemoryManager()
      : enforce_memory_policy_pending_(false) {}

  void SetNow(base::Time now) { now_ = now; }

  void set_enforce_memory_policy_pending(bool enforce_memory_policy_pending) {
    enforce_memory_policy_pending_ = enforce_memory_policy_pending;
  }
  bool enforce_memory_policy_pending() const {
    return enforce_memory_policy_pending_;
  }

 private:
  // Overriden from DiscardableSharedMemoryManager:
  base::Time Now() const override { return now_; }
  void ScheduleEnforceMemoryPolicy() override {
    enforce_memory_policy_pending_ = true;
  }

  base::Time now_;
  bool enforce_memory_policy_pending_;
};

class DiscardableSharedMemoryManagerTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    manager_.reset(new TestDiscardableSharedMemoryManager);
  }

  // DiscardableSharedMemoryManager requires a message loop.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<TestDiscardableSharedMemoryManager> manager_;
};

TEST_F(DiscardableSharedMemoryManagerTest, AllocateForClient) {
  const int kDataSize = 1024;
  uint8_t data[kDataSize];
  memset(data, 0x80, kDataSize);

  base::UnsafeSharedMemoryRegion shared_region;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 0, &shared_region);
  ASSERT_TRUE(shared_region.IsValid());

  TestDiscardableSharedMemory memory(std::move(shared_region));
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  memcpy(memory.memory(), data, kDataSize);
  memory.SetNow(base::Time::FromDoubleT(1));
  memory.Unlock(0, 0);

  ASSERT_EQ(base::DiscardableSharedMemory::SUCCESS, memory.Lock(0, 0));
  EXPECT_EQ(memcmp(data, memory.memory(), kDataSize), 0);
  memory.Unlock(0, 0);
}

TEST_F(DiscardableSharedMemoryManagerTest, Purge) {
  const int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region1;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 1, &shared_region1);
  ASSERT_TRUE(shared_region1.IsValid());

  TestDiscardableSharedMemory memory1(std::move(shared_region1));
  bool rv = memory1.Map(kDataSize);
  ASSERT_TRUE(rv);

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());

  TestDiscardableSharedMemory memory2(std::move(shared_region2));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Enough memory for both allocations.
  manager_->SetNow(base::Time::FromDoubleT(1));
  manager_->SetMemoryLimit(memory1.mapped_size() + memory2.mapped_size());

  memory1.SetNow(base::Time::FromDoubleT(2));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromDoubleT(2));
  memory2.Unlock(0, 0);

  // Manager should not have to schedule another call to EnforceMemoryPolicy().
  manager_->SetNow(base::Time::FromDoubleT(3));
  manager_->EnforceMemoryPolicy();
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  // Memory should still be resident.
  EXPECT_TRUE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());

  auto lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);
  lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);

  memory1.SetNow(base::Time::FromDoubleT(4));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromDoubleT(5));
  memory2.Unlock(0, 0);

  // Just enough memory for one allocation.
  manager_->SetNow(base::Time::FromDoubleT(6));
  manager_->SetMemoryLimit(memory2.mapped_size());
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  // LRU allocation should still be resident.
  EXPECT_FALSE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());

  lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::FAILED, lock_rv);
  lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);
}

TEST_F(DiscardableSharedMemoryManagerTest, EnforceMemoryPolicy) {
  const int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 0, &shared_region);
  ASSERT_TRUE(shared_region.IsValid());

  TestDiscardableSharedMemory memory(std::move(shared_region));
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Not enough memory for one allocation.
  manager_->SetNow(base::Time::FromDoubleT(1));
  manager_->SetMemoryLimit(memory.mapped_size() - 1);
  // We need to enforce memory policy as our memory usage is currently above
  // the limit.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  manager_->set_enforce_memory_policy_pending(false);
  manager_->SetNow(base::Time::FromDoubleT(2));
  manager_->EnforceMemoryPolicy();
  // Still need to enforce memory policy as nothing can be purged.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  memory.SetNow(base::Time::FromDoubleT(3));
  memory.Unlock(0, 0);

  manager_->set_enforce_memory_policy_pending(false);
  manager_->SetNow(base::Time::FromDoubleT(4));
  manager_->EnforceMemoryPolicy();
  // Memory policy should have successfully been enforced.
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  EXPECT_EQ(base::DiscardableSharedMemory::FAILED, memory.Lock(0, 0));
}

TEST_F(DiscardableSharedMemoryManagerTest,
       ReduceMemoryAfterSegmentHasBeenDeleted) {
  const int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region1;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 1, &shared_region1);
  ASSERT_TRUE(shared_region1.IsValid());

  TestDiscardableSharedMemory memory1(std::move(shared_region1));
  bool rv = memory1.Map(kDataSize);
  ASSERT_TRUE(rv);

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());

  TestDiscardableSharedMemory memory2(std::move(shared_region2));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Unlock and delete segment 1.
  memory1.SetNow(base::Time::FromDoubleT(1));
  memory1.Unlock(0, 0);
  memory1.Unmap();
  memory1.Close();
  manager_->ClientDeletedDiscardableSharedMemory(1, kInvalidUniqueID);

  // Make sure the manager is able to reduce memory after the segment 1 was
  // deleted.
  manager_->SetNow(base::Time::FromDoubleT(2));
  manager_->SetMemoryLimit(0);

  // Unlock segment 2.
  memory2.SetNow(base::Time::FromDoubleT(3));
  memory2.Unlock(0, 0);
}

class DiscardableSharedMemoryManagerScheduleEnforceMemoryPolicyTest
    : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override { manager_.reset(new DiscardableSharedMemoryManager); }

  // DiscardableSharedMemoryManager requires a message loop.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<DiscardableSharedMemoryManager> manager_;
};

class SetMemoryLimitRunner : public base::DelegateSimpleThread::Delegate {
 public:
  SetMemoryLimitRunner(DiscardableSharedMemoryManager* manager, size_t limit)
      : manager_(manager), limit_(limit) {}
  ~SetMemoryLimitRunner() override {}

  void Run() override { manager_->SetMemoryLimit(limit_); }

 private:
  DiscardableSharedMemoryManager* const manager_;
  const size_t limit_;
};

TEST_F(DiscardableSharedMemoryManagerScheduleEnforceMemoryPolicyTest,
       SetMemoryLimitOnSimpleThread) {
  const int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 0, &shared_region);
  ASSERT_TRUE(shared_region.IsValid());

  // Set the memory limit to a value that will require EnforceMemoryPolicy()
  // to be schedule on a thread without a message loop.
  SetMemoryLimitRunner runner(manager_.get(), kDataSize - 1);
  base::DelegateSimpleThread thread(&runner, "memory_limit_setter");
  thread.Start();
  thread.Join();
}

}  // namespace
}  // namespace discardable_memory
