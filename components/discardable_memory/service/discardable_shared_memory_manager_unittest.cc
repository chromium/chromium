// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/service/discardable_shared_memory_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>

#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

constexpr int kInvalidUniqueID = -1;

class TestDiscardableSharedMemory : public base::DiscardableSharedMemory {
 public:
  TestDiscardableSharedMemory() = default;

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
  TestDiscardableSharedMemoryManager() = default;

  void SetNow(base::Time now) { now_ = now; }

  void set_enforce_memory_policy_pending(bool enforce_memory_policy_pending) {
    enforce_memory_policy_pending_ = enforce_memory_policy_pending;
  }
  bool enforce_memory_policy_pending() const {
    return enforce_memory_policy_pending_;
  }

  size_t on_memory_pressure_call_count() const {
    return on_memory_pressure_call_count_;
  }

 private:
  // Overriden from DiscardableSharedMemoryManager:
  void OnMemoryPressure(
      base::MemoryPressureLevel memory_pressure_level) override {
    DiscardableSharedMemoryManager::OnMemoryPressure(memory_pressure_level);
    ++on_memory_pressure_call_count_;
  }
  base::Time Now() const override { return now_; }
  void ScheduleEnforceMemoryPolicy() override {
    enforce_memory_policy_pending_ = true;
  }

  base::Time now_;
  bool enforce_memory_policy_pending_ = false;
  size_t on_memory_pressure_call_count_ = 0;
};

class DiscardableSharedMemoryManagerTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<TestDiscardableSharedMemoryManager>();
  }

  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  // DiscardableSharedMemoryManager requires a message loop and a worker thread.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestDiscardableSharedMemoryManager> manager_;
};

TEST_F(DiscardableSharedMemoryManagerTest, AllocateForClient) {
  constexpr int kDataSize = 1024;
  std::array<uint8_t, kDataSize> data;
  data.fill(0x80);

  base::UnsafeSharedMemoryRegion shared_region;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 0, &shared_region);
  ASSERT_TRUE(shared_region.IsValid());

  TestDiscardableSharedMemory memory(std::move(shared_region));
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  memory.memory().copy_from(data);
  memory.SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  memory.Unlock(0, 0);

  ASSERT_EQ(base::DiscardableSharedMemory::SUCCESS, memory.Lock(0, 0));
  EXPECT_EQ(data, memory.memory());
  memory.Unlock(0, 0);
}

TEST_F(DiscardableSharedMemoryManagerTest, Purge) {
  constexpr int kDataSize = 1024;

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
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  manager_->SetMemoryLimit(memory1.mapped_size() + memory2.mapped_size());

  memory1.SetNow(base::Time::FromSecondsSinceUnixEpoch(2));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromSecondsSinceUnixEpoch(2));
  memory2.Unlock(0, 0);

  // Manager should not have to schedule another call to EnforceMemoryPolicy().
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(3));
  manager_->EnforceMemoryPolicy();
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  // Memory should still be resident.
  EXPECT_TRUE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());

  auto lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);
  lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);

  memory1.SetNow(base::Time::FromSecondsSinceUnixEpoch(4));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromSecondsSinceUnixEpoch(5));
  memory2.Unlock(0, 0);

  // Just enough memory for one allocation.
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(6));
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
  constexpr int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 0, &shared_region);
  ASSERT_TRUE(shared_region.IsValid());

  TestDiscardableSharedMemory memory(std::move(shared_region));
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Not enough memory for one allocation.
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  manager_->SetMemoryLimit(memory.mapped_size() - 1);
  // We need to enforce memory policy as our memory usage is currently above
  // the limit.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  manager_->set_enforce_memory_policy_pending(false);
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(2));
  manager_->EnforceMemoryPolicy();
  // Still need to enforce memory policy as nothing can be purged.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  memory.SetNow(base::Time::FromSecondsSinceUnixEpoch(3));
  memory.Unlock(0, 0);

  manager_->set_enforce_memory_policy_pending(false);
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(4));
  manager_->EnforceMemoryPolicy();
  // Memory policy should have successfully been enforced.
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  EXPECT_EQ(base::DiscardableSharedMemory::FAILED, memory.Lock(0, 0));
}

TEST_F(DiscardableSharedMemoryManagerTest,
       ReduceMemoryAfterSegmentHasBeenDeleted) {
  constexpr int kDataSize = 1024;

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
  memory1.SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  memory1.Unlock(0, 0);
  memory1.Unmap();
  memory1.Close();
  manager_->ClientDeletedDiscardableSharedMemory(1, kInvalidUniqueID);

  // Make sure the manager is able to reduce memory after the segment 1 was
  // deleted.
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(2));
  manager_->SetMemoryLimit(0);

  // Unlock segment 2.
  memory2.SetNow(base::Time::FromSecondsSinceUnixEpoch(3));
  memory2.Unlock(0, 0);
}

TEST_F(DiscardableSharedMemoryManagerTest, OnModerateMemoryPressure) {
  constexpr int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region1;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 1, &shared_region1);
  ASSERT_TRUE(shared_region1.IsValid());
  TestDiscardableSharedMemory memory1(std::move(shared_region1));
  ASSERT_TRUE(memory1.Map(kDataSize));

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());
  TestDiscardableSharedMemory memory2(std::move(shared_region2));
  ASSERT_TRUE(memory2.Map(kDataSize));

  // Allow two segments to be resident so moderate pressure should trim to one.
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  manager_->SetMemoryLimit(memory1.mapped_size() + memory2.mapped_size());
  memory1.SetNow(base::Time::FromSecondsSinceUnixEpoch(2));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromSecondsSinceUnixEpoch(3));
  memory2.Unlock(0, 0);
  // Manager time must be after all segment unlock times for eviction to work.
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(4));

  base::MemoryPressureListenerRegistry::NotifyMemoryPressure(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  if (manager_->memory_pressure_level() !=
      base::MEMORY_PRESSURE_LEVEL_MODERATE) {
    GTEST_SKIP() << "Moderate memory pressure notifications are suppressed for "
                    "kDiscardableSharedMemoryManager.";
  }

  task_environment_.RunUntilIdle();

  EXPECT_EQ(memory2.mapped_size(), manager_->GetBytesAllocated());
  EXPECT_FALSE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());
  EXPECT_EQ(1u, manager_->on_memory_pressure_call_count());
}

TEST_F(DiscardableSharedMemoryManagerTest, OnCriticalMemoryPressure) {
  constexpr int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region1;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 1, &shared_region1);
  ASSERT_TRUE(shared_region1.IsValid());
  TestDiscardableSharedMemory memory1(std::move(shared_region1));
  ASSERT_TRUE(memory1.Map(kDataSize));

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());
  TestDiscardableSharedMemory memory2(std::move(shared_region2));
  ASSERT_TRUE(memory2.Map(kDataSize));

  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  manager_->SetMemoryLimit(memory1.mapped_size() + memory2.mapped_size());
  memory1.SetNow(base::Time::FromSecondsSinceUnixEpoch(2));
  memory1.Unlock(0, 0);
  memory2.SetNow(base::Time::FromSecondsSinceUnixEpoch(3));
  memory2.Unlock(0, 0);
  // Manager time must be after all segment unlock times for eviction to work.
  manager_->SetNow(base::Time::FromSecondsSinceUnixEpoch(4));

  base::MemoryPressureListenerRegistry::NotifyMemoryPressure(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  if (manager_->memory_pressure_level() !=
      base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    GTEST_SKIP() << "Critical memory pressure notifications are suppressed for "
                    "kDiscardableSharedMemoryManager.";
  }

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, manager_->GetBytesAllocated());
  EXPECT_FALSE(memory1.IsMemoryResident());
  EXPECT_FALSE(memory2.IsMemoryResident());
  EXPECT_EQ(1u, manager_->on_memory_pressure_call_count());
}

class DiscardableSharedMemoryManagerScheduleEnforceMemoryPolicyTest
    : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<DiscardableSharedMemoryManager>();
  }

  // DiscardableSharedMemoryManager requires a message loop and a worker thread.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DiscardableSharedMemoryManager> manager_;
};

class SetMemoryLimitRunner : public base::DelegateSimpleThread::Delegate {
 public:
  SetMemoryLimitRunner(DiscardableSharedMemoryManager* manager, size_t limit)
      : manager_(manager), limit_(limit) {}
  ~SetMemoryLimitRunner() override = default;

  void Run() override { manager_->SetMemoryLimit(limit_); }

 private:
  const raw_ptr<DiscardableSharedMemoryManager> manager_;
  const size_t limit_;
};

TEST_F(DiscardableSharedMemoryManagerScheduleEnforceMemoryPolicyTest,
       SetMemoryLimitOnSimpleThread) {
  constexpr int kDataSize = 1024;

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
