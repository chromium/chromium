// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"

#include <limits>

#include "base/byte_size.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/page_size.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/memory_coordinator/utils.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

using base::Location;
using base::OnceClosure;

class TestSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
  ~TestSingleThreadTaskRunner() override = default;
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       base::TimeDelta delay) override {
    return true;
  }
  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  base::TimeDelta delay) override {
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }
};

class TestClientDiscardableSharedMemoryManager
    : public ClientDiscardableSharedMemoryManager {
 public:
  TestClientDiscardableSharedMemoryManager()
      : ClientDiscardableSharedMemoryManager(
            base::MakeRefCounted<TestSingleThreadTaskRunner>()) {}

  std::unique_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size, int32_t id) override {
    // Like the real IPC, only sizes that fit in a uint32_t can be requested.
    EXPECT_TRUE(base::IsValueInRangeForNumericType<uint32_t>(size));
    auto shared_memory = std::make_unique<base::DiscardableSharedMemory>();
    if (!shared_memory->CreateAndMap(size)) {
      return nullptr;
    }
    return shared_memory;
  }

  void DeletedDiscardableSharedMemory(int32_t id) override {}

  size_t GetFreelistSize() const {
    base::AutoLock lock(lock_);
    return heap_->GetFreelistSize();
  }

  bool IsPurgeScheduled() const {
    base::AutoLock lock(lock_);
    return is_purge_scheduled_;
  }

 private:
  ~TestClientDiscardableSharedMemoryManager() override = default;
};

class ClientDiscardableSharedMemoryManagerTest : public testing::Test {
 public:
  ClientDiscardableSharedMemoryManagerTest()
      : task_env_(base::test::TaskEnvironment::MainThreadType::UI,
                  base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void NotifyUpdateMemoryLimitAndReleaseAndRun(base::MemoryLimit memory_limit) {
    base::RunLoop run_loop;
    test_registry_.NotifyUpdateMemoryLimitAndReleaseAsync(
        memory_limit, run_loop.QuitClosure());
    run_loop.Run();
  }

  const base::ByteSize page_size_{base::GetPageSize()};
  base::test::TaskEnvironment task_env_;
  base::TestMemoryConsumerRegistry test_registry_;
};

// This test allocates a single piece of memory, then verifies that calling
// |BackgroundPurge| only affects the memory when it is unlocked.
TEST_F(ClientDiscardableSharedMemoryManagerTest, Simple) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  // Initially, we should have no memory allocated
  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem = client->AllocateLockedDiscardableMemory(page_size_);

  // After allocation, we should have allocated a single piece of memory.
  EXPECT_EQ(client->GetAllocatedSize(), page_size_);

  client->BackgroundPurge();

  // All our memory is locked, so calling |BackgroundPurge| should have no
  // effect.
  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(base::GetPageSize()));

  mem->Unlock();

  // Unlocking has no effect on the amount of memory we have allocated.
  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(base::GetPageSize()));

  client->BackgroundPurge();

  // Now that |mem| is unlocked, the call to |BackgroundPurge| will
  // remove it.
  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
}

// This test allocates multiple pieces of memory, then unlocks them one by one,
// verifying that |BackgroundPurge| only affects the unlocked pieces of
// memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, MultipleOneByOne) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 22 / 10);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 11 / 10);
  auto mem3 = client->AllocateLockedDiscardableMemory(page_size_ * 35 / 10);
  auto mem4 = client->AllocateLockedDiscardableMemory(page_size_ * 2 / 10);

  EXPECT_EQ(client->GetAllocatedSize(), 10 * page_size_);

  // Does nothing because everything is locked.
  client->BackgroundPurge();

  EXPECT_EQ(client->GetAllocatedSize(), 10 * page_size_);

  mem1->Unlock();

  // Does nothing, since we don't have any free memory, just unlocked memory.
  client->ReleaseFreeMemory();

  EXPECT_EQ(client->GetAllocatedSize(), 10 * page_size_);

  // This gets rid of |mem1| (which is unlocked), but not the rest of the
  // memory.
  client->BackgroundPurge();

  EXPECT_EQ(client->GetAllocatedSize(), 7 * page_size_);

  // We do similar checks to above for the rest of the memory.
  mem2->Unlock();

  client->BackgroundPurge();

  EXPECT_EQ(client->GetAllocatedSize(), 5 * page_size_);

  mem3->Unlock();

  client->BackgroundPurge();
  EXPECT_EQ(client->GetAllocatedSize(), 1 * page_size_);

  mem4->Unlock();

  client->BackgroundPurge();
  EXPECT_EQ(client->GetAllocatedSize(), 0 * page_size_);
}

// This test allocates multiple pieces of memory, then unlocks them all,
// verifying that |BackgroundPurge| only affects the unlocked pieces of
// memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, MultipleAtOnce) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 22 / 10);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 11 / 10);
  auto mem3 = client->AllocateLockedDiscardableMemory(page_size_ * 35 / 10);
  auto mem4 = client->AllocateLockedDiscardableMemory(page_size_ * 2 / 10);

  EXPECT_EQ(client->GetAllocatedSize(), 10 * page_size_);

  // Does nothing because everything is locked.
  client->BackgroundPurge();

  EXPECT_EQ(client->GetAllocatedSize(), 10 * page_size_);

  // Unlock all pieces of memory at once.
  mem1->Unlock();
  mem2->Unlock();
  mem3->Unlock();
  mem4->Unlock();

  client->BackgroundPurge();
  EXPECT_EQ(client->GetAllocatedSize(), 0 * page_size_);
}

// Tests that FreeLists are only released once all memory has been released.
TEST_F(ClientDiscardableSharedMemoryManagerTest, Release) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 2);

  size_t freelist_size = client->GetFreelistSize();
  EXPECT_EQ(client->GetAllocatedSize(), 5 * page_size_);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client->GetAllocatedSize(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(),
            freelist_size + page_size_.InBytes() * 3);

  client->BackgroundPurge();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client->GetAllocatedSize(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(),
            freelist_size + page_size_.InBytes() * 3);

  mem2 = nullptr;

  // No memory is allocated, but freelists are grown.
  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  EXPECT_EQ(client->GetFreelistSize(),
            freelist_size + page_size_.InBytes() * 5);

  client->BackgroundPurge();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  EXPECT_EQ(client->GetFreelistSize(), 0u);
}

// Similar to previous test, but makes sure that freelist still shrinks when
// last piece of memory was just unlocked instead of released.
TEST_F(ClientDiscardableSharedMemoryManagerTest, ReleaseUnlocked) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 2);

  size_t freelist_size = client->GetFreelistSize();
  EXPECT_EQ(client->GetAllocatedSize(), 5 * page_size_);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client->GetAllocatedSize(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(),
            freelist_size + page_size_.InBytes() * 3);

  client->BackgroundPurge();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client->GetAllocatedSize(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(),
            freelist_size + page_size_.InBytes() * 3);

  mem2->Unlock();

  // No change in memory usage, since memory was only unlocked not released.
  EXPECT_EQ(client->GetAllocatedSize(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(),
            freelist_size + page_size_.InBytes() * 3);

  client->BackgroundPurge();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  EXPECT_EQ(client->GetFreelistSize(), 0u);
}

// This tests that memory is actually removed by the periodic purging. We mock a
// task runner for this test and fast forward to make sure that the memory is
// purged at the right time.
TEST_F(ClientDiscardableSharedMemoryManagerTest, ScheduledReleaseUnlocked) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetAllocatedSize(), 3 * page_size_);

  mem1->Unlock();

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
}

// Same as the above test, but tests that multiple pieces of memory will be
// handled properly.
TEST_F(ClientDiscardableSharedMemoryManagerTest,
       ScheduledReleaseUnlockedMultiple) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 2);

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetAllocatedSize(), 5 * page_size_);

  mem1->Unlock();

  EXPECT_EQ(client->GetAllocatedSize(), 5 * page_size_);

  // The purge only removes things that have been unlocked for at least
  // |kMinAgeForScheduledPurge|
  // minutes so this shouldn't remove anything.
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge / 2);

  EXPECT_EQ(client->GetAllocatedSize(), 5 * page_size_);

  // The periodic purge should remove anything that's been locked for over
  // |kMinAgeForScheduledPurge|
  // minutes, so fast forward slightly more so that it happens.
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge / 2);

  EXPECT_EQ(client->GetAllocatedSize(), 2 * page_size_);

  mem2->Unlock();

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  EXPECT_EQ(client->GetFreelistSize(), 0u);
}

// Test that a repeating timer for background purging is created when we
// allocate memory and discarded when we run out of allocated memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, SchedulingProactivePurging) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_FALSE(client->IsPurgeScheduled());

  // the amount of memory allocated here is arbitrary, we're only trying to get
  // the timer started.
  auto mem = client->AllocateLockedDiscardableMemory(base::ByteSize(200));
  EXPECT_TRUE(client->IsPurgeScheduled());

  // This does not stop periodic purge because there is still memory allocated.
  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  // Even though no memory is allocated, periodic purge will stop running after
  // the next scheduled task.
  mem = nullptr;
  EXPECT_TRUE(client->IsPurgeScheduled());
  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval);
  EXPECT_FALSE(client->IsPurgeScheduled());
}

// This test is similar to the one above, but tests that creating and deleting
// the timer still works with multiple pieces of allocated memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest,
       SchedulingProactivePurgingMultipleAllocations) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_FALSE(client->IsPurgeScheduled());

  // the amount of memory allocated here is arbitrary, we're only trying to get
  // the timer started.
  auto mem = client->AllocateLockedDiscardableMemory(base::ByteSize(200));
  auto mem2 = client->AllocateLockedDiscardableMemory(base::ByteSize(100));

  task_env_.FastForwardBy(base::Seconds(0));
  EXPECT_TRUE(client->IsPurgeScheduled());

  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  mem = nullptr;
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval);
  EXPECT_TRUE(client->IsPurgeScheduled());

  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  mem2 = nullptr;
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval);
  EXPECT_FALSE(client->IsPurgeScheduled());
}

TEST_F(ClientDiscardableSharedMemoryManagerTest, OnReleaseMemory) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();

  ASSERT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);
  mem1 = nullptr;

  // Because the manager allocates memory in larger chunks (e.g. 4MB) and carves
  // out pages, the freelist already contains the rest of the chunk. We capture
  // the exact size to assert that Moderate pressure frees absolutely nothing.
  const size_t expected_freelist_size = client->GetFreelistSize();
  EXPECT_GT(expected_freelist_size, 0u);

  // At moderate pressure, memory should NOT be released.
  NotifyUpdateMemoryLimitAndReleaseAndRun(
      base::MemoryLimit::ModeratePressureThreshold());
  EXPECT_EQ(client->GetFreelistSize(), expected_freelist_size);

  // At critical pressure, memory SHOULD be released.
  NotifyUpdateMemoryLimitAndReleaseAndRun(
      base::MemoryLimit::CriticalPressureThreshold());
  EXPECT_EQ(client->GetFreelistSize(), 0u);
}

// Verifies that sizes near ByteSize::Max() do not wrap around when rounded up
// to a page multiple, which would result in an undersized allocation.
TEST_F(ClientDiscardableSharedMemoryManagerTest, HugeAllocationFails) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();

  constexpr base::ByteSize kMax = base::ByteSize::Max();
  for (base::ByteSize size : {kMax, kMax - page_size_ + base::ByteSize(2)}) {
    auto mem = client->AllocateLockedDiscardableMemory(size);
    EXPECT_FALSE(mem);
    EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
  }
}

#if defined(ARCH_CPU_64_BITS)
// Verifies that allocations larger than the browser process can be asked for
// fail instead of being truncated.
TEST_F(ClientDiscardableSharedMemoryManagerTest, AllocationAboveIpcLimitFails) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();

  auto mem = client->AllocateLockedDiscardableMemory(base::GiB(4) + page_size_);
  EXPECT_FALSE(mem);
  EXPECT_EQ(client->GetAllocatedSize(), base::ByteSize(0));
}
#endif  // defined(ARCH_CPU_64_BITS)

TEST_F(ClientDiscardableSharedMemoryManagerTest, ByteSizeApi) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  base::DiscardableMemoryAllocator::SetInstance(client.get());
  base::ScopedClosureRunner reset_allocator(base::BindOnce(
      [] { base::DiscardableMemoryAllocator::SetInstance(nullptr); }));
  base::DiscardableMemoryAllocator* allocator =
      base::DiscardableMemoryAllocator::GetInstance();

  EXPECT_EQ(allocator->GetAllocatedSize(), base::ByteSize(0));

  client->SetBytesAllocatedLimitForTesting(base::MiB(4));

  auto mem1 = client->AllocateLockedDiscardableMemory(base::MiB(4));
  ASSERT_TRUE(mem1);
  EXPECT_EQ(client->GetAllocatedSize(), base::MiB(4));
  EXPECT_EQ(allocator->GetAllocatedSize(), base::MiB(4));

  auto mem2 = allocator->AllocateLockedDiscardableMemory(page_size_);
  EXPECT_FALSE(mem2);

  auto mem3 = allocator->AllocateLockedDiscardableMemoryWithRetryOrDie(
      page_size_, base::BindLambdaForTesting([&] { mem1.reset(); }));
  EXPECT_TRUE(mem3);
  EXPECT_FALSE(mem1);

  client->SetBytesAllocatedLimitForTesting(base::ByteSize(0));
}

}  // namespace
}  // namespace discardable_memory
