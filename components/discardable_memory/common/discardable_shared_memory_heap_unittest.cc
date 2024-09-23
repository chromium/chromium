// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/common/discardable_shared_memory_heap.h"

#include <inttypes.h>
#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/page_size.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

void NullTask() {}

TEST(DiscardableSharedMemoryHeapTest, Basic) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  // Initial size should be 0.
  EXPECT_EQ(0u, heap.GetSize());

  // Initial size of free lists should be 0.
  EXPECT_EQ(0u, heap.GetFreelistSize());

  // Free lists are initially empty.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  const size_t kBlocks = 10;
  size_t memory_size = block_size * kBlocks;
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));

  // Create new span for memory.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(std::move(memory), memory_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask)));

  // Size should match |memory_size|.
  EXPECT_EQ(memory_size, heap.GetSize());

  // Size of free lists should still be 0.
  EXPECT_EQ(0u, heap.GetFreelistSize());

  // Free list should still be empty as |new_span| is currently in use.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  // Done using |new_span|. Merge it into the free lists.
  heap.MergeIntoFreeLists(std::move(new_span));

  // Size of free lists should now match |memory_size|.
  EXPECT_EQ(memory_size, heap.GetFreelistSize());

  // Free lists should not contain a span that is larger than kBlocks.
  EXPECT_FALSE(heap.SearchFreeLists(kBlocks + 1, 0));

  // Free lists should contain a span that satisfies the request for kBlocks.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.SearchFreeLists(kBlocks, 0);
  ASSERT_TRUE(span);

  // Free lists should be empty again.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  // Merge it into the free lists again.
  heap.MergeIntoFreeLists(std::move(span));
}

TEST(DiscardableSharedMemoryHeapTest, SplitAndMerge) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  const size_t kBlocks = 6;
  size_t memory_size = block_size * kBlocks;
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(std::move(memory), memory_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask)));

  // Split span into two.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> leftover =
      heap.Split(new_span.get(), 3);
  ASSERT_TRUE(leftover);

  // Merge |leftover| into free lists.
  heap.MergeIntoFreeLists(std::move(leftover));

  // Some of the memory is still in use.
  EXPECT_FALSE(heap.SearchFreeLists(kBlocks, 0));

  // Merge |span| into free lists.
  heap.MergeIntoFreeLists(std::move(new_span));

  // Remove a 2 page span from free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span1 =
      heap.SearchFreeLists(2, kBlocks);
  ASSERT_TRUE(span1);

  // Remove another 2 page span from free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span2 =
      heap.SearchFreeLists(2, kBlocks);
  ASSERT_TRUE(span2);

  // Merge |span1| back into free lists.
  heap.MergeIntoFreeLists(std::move(span1));

  // Some of the memory is still in use.
  EXPECT_FALSE(heap.SearchFreeLists(kBlocks, 0));

  // Merge |span2| back into free lists.
  heap.MergeIntoFreeLists(std::move(span2));

  // All memory has been returned to the free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> large_span =
      heap.SearchFreeLists(kBlocks, 0);
  ASSERT_TRUE(large_span);

  // Merge it into the free lists again.
  heap.MergeIntoFreeLists(std::move(large_span));
}

TEST(DiscardableSharedMemoryHeapTest, MergeSingleBlockSpan) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  const size_t kBlocks = 6;
  size_t memory_size = block_size * kBlocks;
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(std::move(memory), memory_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask)));

  // Split span into two.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> leftover =
      heap.Split(new_span.get(), 5);
  ASSERT_TRUE(leftover);

  // Merge |new_span| into free lists.
  heap.MergeIntoFreeLists(std::move(new_span));

  // Merge |leftover| into free lists.
  heap.MergeIntoFreeLists(std::move(leftover));
}

TEST(DiscardableSharedMemoryHeapTest, Grow) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory1(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory1->CreateAndMap(block_size));
  heap.MergeIntoFreeLists(heap.Grow(std::move(memory1), block_size,
                                    next_discardable_shared_memory_id++,
                                    base::BindOnce(NullTask)));

  // Remove a span from free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span1 =
      heap.SearchFreeLists(1, 0);
  EXPECT_TRUE(span1);

  // No more memory available.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  // Grow free lists using new memory.
  std::unique_ptr<base::DiscardableSharedMemory> memory2(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory2->CreateAndMap(block_size));
  heap.MergeIntoFreeLists(heap.Grow(std::move(memory2), block_size,
                                    next_discardable_shared_memory_id++,
                                    base::BindOnce(NullTask)));

  // Memory should now be available.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span2 =
      heap.SearchFreeLists(1, 0);
  EXPECT_TRUE(span2);

  // Merge spans into the free lists again.
  heap.MergeIntoFreeLists(std::move(span1));
  heap.MergeIntoFreeLists(std::move(span2));
}

TEST(DiscardableSharedMemoryHeapTest, ReleaseFreeMemory) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(std::move(memory), block_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask));

  // Free lists should be empty.
  EXPECT_EQ(0u, heap.GetFreelistSize());

  heap.ReleaseFreeMemory();

  // Size should still match |block_size|.
  EXPECT_EQ(block_size, heap.GetSize());

  heap.MergeIntoFreeLists(std::move(span));
  heap.ReleaseFreeMemory();

  // Memory should have been released.
  EXPECT_EQ(0u, heap.GetSize());
  EXPECT_EQ(0u, heap.GetFreelistSize());
}

TEST(DiscardableSharedMemoryHeapTest, ReleasePurgedMemory) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(std::move(memory), block_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask));

  // Unlock memory so it can be purged.
  span->shared_memory()->Unlock(0, 0);

  // Purge and release shared memory.
  bool rv = span->shared_memory()->Purge(base::Time::Now());
  EXPECT_TRUE(rv);
  heap.ReleasePurgedMemory();

  // Shared memory backing for |span| should be gone.
  EXPECT_FALSE(span->shared_memory());

  // Size should be 0.
  EXPECT_EQ(0u, heap.GetSize());
}

TEST(DiscardableSharedMemoryHeapTest, Slack) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  const size_t kBlocks = 6;
  size_t memory_size = block_size * kBlocks;
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  heap.MergeIntoFreeLists(heap.Grow(std::move(memory), memory_size,
                                    next_discardable_shared_memory_id++,
                                    base::BindOnce(NullTask)));

  // No free span that is less or equal to 3 + 1.
  EXPECT_FALSE(heap.SearchFreeLists(3, 1));

  // No free span that is less or equal to 3 + 2.
  EXPECT_FALSE(heap.SearchFreeLists(3, 2));

  // No free span that is less or equal to 1 + 4.
  EXPECT_FALSE(heap.SearchFreeLists(1, 4));

  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.SearchFreeLists(1, 5);
  EXPECT_TRUE(span);

  heap.MergeIntoFreeLists(std::move(span));
}

void OnDeleted(bool* deleted) {
  *deleted = true;
}

TEST(DiscardableSharedMemoryHeapTest, DeletedCallback) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  bool deleted = false;
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span = heap.Grow(
      std::move(memory), block_size, next_discardable_shared_memory_id++,
      base::BindOnce(OnDeleted, base::Unretained(&deleted)));

  heap.MergeIntoFreeLists(std::move(span));
  heap.ReleaseFreeMemory();

  EXPECT_TRUE(deleted);
}

TEST(DiscardableSharedMemoryHeapTest, CreateMemoryAllocatorDumpTest) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap;

  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(std::move(memory), block_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask));

  // Check if allocator dump is created when span exists.
  std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd(
      new base::trace_event::ProcessMemoryDump(
          {base::trace_event::MemoryDumpLevelOfDetail::kDetailed}));
  EXPECT_TRUE(heap.CreateMemoryAllocatorDump(span.get(), "discardable/test1",
                                             pmd.get()));

  // Unlock, Purge and release shared memory.
  span->shared_memory()->Unlock(0, 0);
  bool rv = span->shared_memory()->Purge(base::Time::Now());
  EXPECT_TRUE(rv);
  heap.ReleasePurgedMemory();

  // Check that allocator dump is created after memory is purged.
  EXPECT_TRUE(heap.CreateMemoryAllocatorDump(span.get(), "discardable/test2",
                                             pmd.get()));
}

TEST(DiscardableSharedMemoryHeapTest, OnMemoryDumpTest) {
  size_t block_size = base::GetPageSize();
  using testing::ByRef;
  using testing::Contains;
  using testing::Eq;
  DiscardableSharedMemoryHeap heap;

  int next_discardable_shared_memory_id = 0;

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kBackground};
  {
    base::trace_event::ProcessMemoryDump pmd(args);
    heap.OnMemoryDump(args, &pmd);
    auto* dump = pmd.GetAllocatorDump(base::StringPrintf(
        "discardable/child_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(&heap)));
    ASSERT_NE(nullptr, dump);

    base::trace_event::MemoryAllocatorDump::Entry freelist_size("freelist_size",
                                                                "bytes", 0);
    base::trace_event::MemoryAllocatorDump::Entry virtual_size("virtual_size",
                                                               "bytes", 0);
    EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(freelist_size))));
    EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(virtual_size))));
  }

  auto memory = std::make_unique<base::DiscardableSharedMemory>();
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  auto span =
      heap.Grow(std::move(memory), block_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask));

  {
    base::trace_event::ProcessMemoryDump pmd(args);
    heap.OnMemoryDump(args, &pmd);
    auto* dump = pmd.GetAllocatorDump(base::StringPrintf(
        "discardable/child_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(&heap)));
    ASSERT_NE(nullptr, dump);

    base::trace_event::MemoryAllocatorDump::Entry freelist_size("freelist_size",
                                                                "bytes", 0);
    base::trace_event::MemoryAllocatorDump::Entry virtual_size(
        "virtual_size", "bytes", block_size);
    EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(freelist_size))));
    EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(virtual_size))));
  }

  {
    heap.MergeIntoFreeLists(std::move(span));

    base::trace_event::ProcessMemoryDump pmd(args);
    heap.OnMemoryDump(args, &pmd);
    auto* dump = pmd.GetAllocatorDump(base::StringPrintf(
        "discardable/child_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(&heap)));
    ASSERT_NE(nullptr, dump);

    base::trace_event::MemoryAllocatorDump::Entry freelist("freelist_size",
                                                           "bytes", block_size);
    base::trace_event::MemoryAllocatorDump::Entry virtual_size(
        "virtual_size", "bytes", block_size);
    EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(freelist))));
    EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(virtual_size))));
  }
}

TEST(DiscardableSharedMemoryHeapTest, DetailedDumpsDontContainRedundantData) {
  using testing::ByRef;
  using testing::Contains;
  using testing::Eq;
  using testing::Not;
  DiscardableSharedMemoryHeap heap;

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  size_t block_size = base::GetPageSize();

  auto memory = std::make_unique<base::DiscardableSharedMemory>();
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  auto span = heap.Grow(std::move(memory), block_size, 1, base::DoNothing());

  base::trace_event::ProcessMemoryDump pmd(args);
  heap.OnMemoryDump(args, &pmd);
  auto* dump = pmd.GetAllocatorDump(base::StringPrintf(
      "discardable/child_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(&heap)));
  ASSERT_NE(nullptr, dump);

  base::trace_event::MemoryAllocatorDump::Entry freelist("freelist_size",
                                                         "bytes", 0);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(freelist))));

  // Detailed dumps do not contain virtual size.
  base::trace_event::MemoryAllocatorDump::Entry virtual_size(
      "virtual_size", "bytes", block_size);
  EXPECT_THAT(dump->entries(), Not(Contains(Eq(ByRef(virtual_size)))));

  heap.MergeIntoFreeLists(std::move(span));
}

}  // namespace
}  // namespace discardable_memory
