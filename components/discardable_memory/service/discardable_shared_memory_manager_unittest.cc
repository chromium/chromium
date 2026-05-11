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
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

constexpr int kInvalidUniqueID = -1;

class TestDiscardableSharedMemoryManager
    : public DiscardableSharedMemoryManager {
 public:
  TestDiscardableSharedMemoryManager() = default;

  void set_enforce_memory_policy_pending(bool enforce_memory_policy_pending) {
    enforce_memory_policy_pending_ = enforce_memory_policy_pending;
  }
  bool enforce_memory_policy_pending() const {
    return enforce_memory_policy_pending_;
  }

 private:
  // Overriden from DiscardableSharedMemoryManager:
  void ScheduleEnforceMemoryPolicy() override {
    enforce_memory_policy_pending_ = true;
  }

  bool enforce_memory_policy_pending_ = false;
};

class DiscardableSharedMemoryManagerTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<TestDiscardableSharedMemoryManager>();
  }

  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  // DiscardableSharedMemoryManager requires a message loop and a worker thread.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

  base::DiscardableSharedMemory memory(std::move(shared_region));
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  memory.memory().copy_from(data);
  task_environment_.FastForwardBy(base::Seconds(1));
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

  base::DiscardableSharedMemory memory1(std::move(shared_region1));
  bool rv = memory1.Map(kDataSize);
  ASSERT_TRUE(rv);

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());

  base::DiscardableSharedMemory memory2(std::move(shared_region2));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Enough memory for both allocations.
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->SetMaxBytes(memory1.mapped_size() + memory2.mapped_size());

  task_environment_.FastForwardBy(base::Seconds(1));
  memory1.Unlock(0, 0);
  memory2.Unlock(0, 0);

  // Manager should not have to schedule another call to EnforceMemoryPolicy().
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->EnforceMemoryPolicy();
  EXPECT_FALSE(manager_->enforce_memory_policy_pending());

  // Memory should still be resident.
  EXPECT_TRUE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());

  auto lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);
  lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(base::DiscardableSharedMemory::SUCCESS, lock_rv);

  task_environment_.FastForwardBy(base::Seconds(1));
  memory1.Unlock(0, 0);
  task_environment_.FastForwardBy(base::Seconds(1));
  memory2.Unlock(0, 0);

  // Just enough memory for one allocation.
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->SetMaxBytes(memory2.mapped_size());
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

  base::DiscardableSharedMemory memory(std::move(shared_region));
  bool rv = memory.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Not enough memory for one allocation.
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->SetMaxBytes(memory.mapped_size() - 1);
  // We need to enforce memory policy as our memory usage is currently above
  // the limit.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  manager_->set_enforce_memory_policy_pending(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->EnforceMemoryPolicy();
  // Still need to enforce memory policy as nothing can be purged.
  EXPECT_TRUE(manager_->enforce_memory_policy_pending());

  task_environment_.FastForwardBy(base::Seconds(1));
  memory.Unlock(0, 0);

  manager_->set_enforce_memory_policy_pending(false);
  task_environment_.FastForwardBy(base::Seconds(1));
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

  base::DiscardableSharedMemory memory1(std::move(shared_region1));
  bool rv = memory1.Map(kDataSize);
  ASSERT_TRUE(rv);

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());

  base::DiscardableSharedMemory memory2(std::move(shared_region2));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Unlock and delete segment 1.
  task_environment_.FastForwardBy(base::Seconds(1));
  memory1.Unlock(0, 0);
  memory1.Unmap();
  memory1.Close();
  manager_->ClientDeletedDiscardableSharedMemory(1, kInvalidUniqueID);

  // Make sure the manager is able to reduce memory after the segment 1 was
  // deleted.
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->SetMaxBytes(0);

  // Unlock segment 2.
  task_environment_.FastForwardBy(base::Seconds(1));
  memory2.Unlock(0, 0);
}

TEST_F(DiscardableSharedMemoryManagerTest, OnModerateMemoryPressure) {
  constexpr int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region1;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 1, &shared_region1);
  ASSERT_TRUE(shared_region1.IsValid());
  base::DiscardableSharedMemory memory1(std::move(shared_region1));
  ASSERT_TRUE(memory1.Map(kDataSize));

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());
  base::DiscardableSharedMemory memory2(std::move(shared_region2));
  ASSERT_TRUE(memory2.Map(kDataSize));

  // Allow two segments to be resident so moderate pressure should trim to one.
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->SetMaxBytes(memory1.mapped_size() + memory2.mapped_size());
  task_environment_.FastForwardBy(base::Seconds(1));
  memory1.Unlock(0, 0);
  task_environment_.FastForwardBy(base::Seconds(1));
  memory2.Unlock(0, 0);
  // Manager time must be after all segment unlock times for eviction to work.
  task_environment_.FastForwardBy(base::Seconds(1));

  manager_->NotifyMemoryPressureAsyncForTesting(
      base::MEMORY_PRESSURE_LEVEL_MODERATE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  EXPECT_EQ(memory2.mapped_size(), manager_->GetBytesAllocated());
  EXPECT_FALSE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());
}

TEST_F(DiscardableSharedMemoryManagerTest, OnCriticalMemoryPressure) {
  constexpr int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region1;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 1, &shared_region1);
  ASSERT_TRUE(shared_region1.IsValid());
  base::DiscardableSharedMemory memory1(std::move(shared_region1));
  ASSERT_TRUE(memory1.Map(kDataSize));

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());
  base::DiscardableSharedMemory memory2(std::move(shared_region2));
  ASSERT_TRUE(memory2.Map(kDataSize));

  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->SetMaxBytes(memory1.mapped_size() + memory2.mapped_size());
  task_environment_.FastForwardBy(base::Seconds(1));
  memory1.Unlock(0, 0);
  task_environment_.FastForwardBy(base::Seconds(1));
  memory2.Unlock(0, 0);
  // Manager time must be after all segment unlock times for eviction to work.
  task_environment_.FastForwardBy(base::Seconds(1));

  manager_->NotifyMemoryPressureAsyncForTesting(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  EXPECT_EQ(0u, manager_->GetBytesAllocated());
  EXPECT_FALSE(memory1.IsMemoryResident());
  EXPECT_FALSE(memory2.IsMemoryResident());
}

class DiscardableSharedMemoryManagerScheduleEnforceMemoryPolicyTest
    : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<DiscardableSharedMemoryManager>();
  }

  // DiscardableSharedMemoryManager requires a message loop and a worker thread.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<DiscardableSharedMemoryManager> manager_;
};

class SetMaxBytesRunner : public base::DelegateSimpleThread::Delegate {
 public:
  SetMaxBytesRunner(DiscardableSharedMemoryManager* manager, size_t bytes)
      : manager_(manager), bytes_(bytes) {}
  ~SetMaxBytesRunner() override = default;

  void Run() override { manager_->SetMaxBytes(bytes_); }

 private:
  const raw_ptr<DiscardableSharedMemoryManager> manager_;
  const size_t bytes_;
};

TEST_F(DiscardableSharedMemoryManagerScheduleEnforceMemoryPolicyTest,
       SetMaxBytesOnSimpleThread) {
  constexpr int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 0, &shared_region);
  ASSERT_TRUE(shared_region.IsValid());

  // Set the max bytes to a value that will require EnforceMemoryPolicy()
  // to be schedule on a thread without a message loop.
  SetMaxBytesRunner runner(manager_.get(), kDataSize - 1);
  base::DelegateSimpleThread thread(&runner, "max_bytes_setter");
  thread.Start();
  thread.Join();
}

TEST_F(DiscardableSharedMemoryManagerTest, OnStatefulMemoryPressure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(base::kStatefulMemoryPressure);

  constexpr int kDataSize = 1024;

  base::UnsafeSharedMemoryRegion shared_region1;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 1, &shared_region1);
  ASSERT_TRUE(shared_region1.IsValid());
  base::DiscardableSharedMemory memory1(std::move(shared_region1));
  ASSERT_TRUE(memory1.Map(kDataSize));

  base::UnsafeSharedMemoryRegion shared_region2;
  manager_->AllocateLockedDiscardableSharedMemoryForClient(
      kInvalidUniqueID, kDataSize, 2, &shared_region2);
  ASSERT_TRUE(shared_region2.IsValid());
  base::DiscardableSharedMemory memory2(std::move(shared_region2));
  ASSERT_TRUE(memory2.Map(kDataSize));

  // Allow two segments to be resident.
  task_environment_.FastForwardBy(base::Seconds(1));
  manager_->SetMaxBytes(memory1.mapped_size() + memory2.mapped_size());
  task_environment_.FastForwardBy(base::Seconds(1));
  memory1.Unlock(0, 0);
  task_environment_.FastForwardBy(base::Seconds(1));
  memory2.Unlock(0, 0);
  // Manager time must be after all segment unlock times for eviction to work.
  task_environment_.FastForwardBy(base::Seconds(1));

  // Moderate pressure should reduce effective limit to 50% (one segment).
  manager_->NotifyMemoryPressureAsyncForTesting(
      base::MEMORY_PRESSURE_LEVEL_MODERATE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  EXPECT_EQ(memory2.mapped_size(), manager_->GetBytesAllocated());
  EXPECT_FALSE(memory1.IsMemoryResident());
  EXPECT_TRUE(memory2.IsMemoryResident());

  // Relieving pressure should restore the limit, but not re-allocate memory.
  manager_->NotifyMemoryPressureAsyncForTesting(
      base::MEMORY_PRESSURE_LEVEL_NONE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  EXPECT_EQ(memory2.mapped_size(), manager_->GetBytesAllocated());

  // Critical pressure should reduce effective limit to 0% (zero segments).
  manager_->NotifyMemoryPressureAsyncForTesting(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  EXPECT_EQ(0u, manager_->GetBytesAllocated());
  EXPECT_FALSE(memory2.IsMemoryResident());
}

}  // namespace
}  // namespace discardable_memory
