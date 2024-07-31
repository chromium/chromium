// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <algorithm>
#include <array>
#include <set>
#include <utility>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/bits.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/page_size.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/gwp_asan.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan {
namespace internal {

static constexpr size_t kMaxMetadata = AllocatorState::kMaxMetadata;
static constexpr size_t kMaxSlots = AllocatorState::kMaxRequestedSlots;

class BaseGpaTest : public testing::Test {
 protected:
  BaseGpaTest(size_t max_allocated_pages,
              size_t max_metadata,
              size_t max_slots,
              bool is_partition_alloc) {
    gpa_.Init(
        AllocatorSettings{
            .max_allocated_pages = max_allocated_pages,
            .num_metadata = max_metadata,
            .total_pages = max_slots,
            .sampling_frequency = 0u,
        },
        base::BindLambdaForTesting(
            [&](size_t allocations) { allocator_oom_ = true; }),
        is_partition_alloc);
  }

  GuardedPageAllocator gpa_;
  bool allocator_oom_ = false;
};

class GuardedPageAllocatorTest : public BaseGpaTest,
                                 public testing::WithParamInterface<bool> {
 protected:
  GuardedPageAllocatorTest()
      : BaseGpaTest(kMaxMetadata, kMaxMetadata, kMaxSlots, GetParam()) {}

  // Get a left- or right- aligned allocation (or nullptr on error.)
  char* GetAlignedAllocation(bool left_aligned, size_t sz, size_t align = 0) {
    for (size_t i = 0; i < 100; i++) {
      void* alloc = gpa_.Allocate(sz, align);
      if (!alloc)
        return nullptr;

      uintptr_t addr = reinterpret_cast<uintptr_t>(alloc);
      bool is_left_aligned =
          (base::bits::AlignUp(addr, base::GetPageSize()) == addr);
      if (is_left_aligned == left_aligned)
        return reinterpret_cast<char*>(addr);

      gpa_.Deallocate(alloc);
    }

    return nullptr;
  }

  // Helper that returns the offset of a right-aligned allocation in the
  // allocation's page.
  uintptr_t GetRightAlignedAllocationOffset(size_t size, size_t align) {
    const uintptr_t page_mask = base::GetPageSize() - 1;

    void* buf = GetAlignedAllocation(false, size, align);
    CHECK(buf);
    gpa_.Deallocate(buf);

    return reinterpret_cast<uintptr_t>(buf) & page_mask;
  }
};

INSTANTIATE_TEST_SUITE_P(VaryPartitionAlloc,
                         GuardedPageAllocatorTest,
                         testing::Values(false, true));

TEST_P(GuardedPageAllocatorTest, SingleAllocDealloc) {
  char* buf = reinterpret_cast<char*>(gpa_.Allocate(base::GetPageSize()));
  EXPECT_NE(buf, nullptr);
  EXPECT_TRUE(gpa_.PointerIsMine(buf));
  memset(buf, 'A', base::GetPageSize());
  EXPECT_DEATH(buf[base::GetPageSize()] = 'A', "");
  gpa_.Deallocate(buf);
  EXPECT_DEATH(buf[0] = 'B', "");
  EXPECT_DEATH(gpa_.Deallocate(buf), "");
}

TEST_P(GuardedPageAllocatorTest, CrashOnBadDeallocPointer) {
  EXPECT_DEATH(gpa_.Deallocate(nullptr), "");
  char* buf = reinterpret_cast<char*>(gpa_.Allocate(8));
  EXPECT_DEATH(gpa_.Deallocate(buf + 1), "");
  gpa_.Deallocate(buf);
}

TEST_P(GuardedPageAllocatorTest, PointerIsMine) {
  void* buf = gpa_.Allocate(1);
  auto malloc_ptr = std::make_unique<char>();
  EXPECT_TRUE(gpa_.PointerIsMine(buf));
  gpa_.Deallocate(buf);
  EXPECT_TRUE(gpa_.PointerIsMine(buf));
  int stack_var;
  EXPECT_FALSE(gpa_.PointerIsMine(&stack_var));
  EXPECT_FALSE(gpa_.PointerIsMine(malloc_ptr.get()));
}

TEST_P(GuardedPageAllocatorTest, GetRequestedSize) {
  void* buf = gpa_.Allocate(100);
  EXPECT_EQ(gpa_.GetRequestedSize(buf), 100U);
#if !BUILDFLAG(IS_APPLE)
  EXPECT_DEATH({ gpa_.GetRequestedSize((char*)buf + 1); }, "");
#else
  EXPECT_EQ(gpa_.GetRequestedSize((char*)buf + 1), 0U);
#endif
}

TEST_P(GuardedPageAllocatorTest, LeftAlignedAllocation) {
  char* buf = GetAlignedAllocation(true, 16);
  ASSERT_NE(buf, nullptr);
  EXPECT_DEATH(buf[-1] = 'A', "");
  buf[0] = 'A';
  buf[base::GetPageSize() - 1] = 'A';
  gpa_.Deallocate(buf);
}

TEST_P(GuardedPageAllocatorTest, RightAlignedAllocation) {
  char* buf =
      GetAlignedAllocation(false, GuardedPageAllocator::kGpaAllocAlignment);
  ASSERT_NE(buf, nullptr);
  buf[-1] = 'A';
  buf[0] = 'A';
  EXPECT_DEATH(buf[GuardedPageAllocator::kGpaAllocAlignment] = 'A', "");
  gpa_.Deallocate(buf);
}

TEST_P(GuardedPageAllocatorTest, AllocationAlignment) {
  const uintptr_t page_size = base::GetPageSize();

  EXPECT_EQ(GetRightAlignedAllocationOffset(9, 1), page_size - 9);
  EXPECT_EQ(GetRightAlignedAllocationOffset(9, 2), page_size - 10);
  EXPECT_EQ(GetRightAlignedAllocationOffset(9, 4), page_size - 12);
  EXPECT_EQ(GetRightAlignedAllocationOffset(9, 8), page_size - 16);

  EXPECT_EQ(GetRightAlignedAllocationOffset(513, 512), page_size - 1024);

  // Default alignment aligns up to the next lowest power of two.
  EXPECT_EQ(GetRightAlignedAllocationOffset(5, 0), page_size - 8);
  EXPECT_EQ(GetRightAlignedAllocationOffset(9, 0), page_size - 16);
  // But only up to 16 bytes.
  EXPECT_EQ(GetRightAlignedAllocationOffset(513, 0), page_size - (512 + 16));

  // We don't support aligning by more than a page.
  EXPECT_EQ(GetAlignedAllocation(false, 5, page_size * 2), nullptr);
}

TEST_P(GuardedPageAllocatorTest, OutOfMemoryCallback) {
  for (size_t i = 0; i < kMaxMetadata; i++)
    EXPECT_NE(gpa_.Allocate(1), nullptr);

  for (size_t i = 0; i < GuardedPageAllocator::kOutOfMemoryCount - 1; i++)
    EXPECT_EQ(gpa_.Allocate(1), nullptr);
  EXPECT_FALSE(allocator_oom_);
  EXPECT_EQ(gpa_.Allocate(1), nullptr);
  EXPECT_TRUE(allocator_oom_);
}

class GuardedPageAllocatorParamTest
    : public BaseGpaTest,
      public testing::WithParamInterface<size_t> {
 protected:
  GuardedPageAllocatorParamTest()
      : BaseGpaTest(GetParam(), kMaxMetadata, kMaxSlots, false) {}
};

TEST_P(GuardedPageAllocatorParamTest, AllocDeallocAllPages) {
  size_t num_allocations = GetParam();
  char* bufs[kMaxMetadata];
  for (size_t i = 0; i < num_allocations; i++) {
    bufs[i] = reinterpret_cast<char*>(gpa_.Allocate(1));
    EXPECT_NE(bufs[i], nullptr);
    EXPECT_TRUE(gpa_.PointerIsMine(bufs[i]));
  }
  EXPECT_EQ(gpa_.Allocate(1), nullptr);
  gpa_.Deallocate(bufs[0]);
  bufs[0] = reinterpret_cast<char*>(gpa_.Allocate(1));
  EXPECT_NE(bufs[0], nullptr);
  EXPECT_TRUE(gpa_.PointerIsMine(bufs[0]));

  // Ensure that no allocation is returned twice.
  std::set<char*> ptr_set;
  for (size_t i = 0; i < num_allocations; i++)
    ptr_set.insert(bufs[i]);
  EXPECT_EQ(ptr_set.size(), num_allocations);

  for (size_t i = 0; i < num_allocations; i++) {
    SCOPED_TRACE(i);
    // Ensure all allocations are valid and writable.
    bufs[i][0] = 'A';
    gpa_.Deallocate(bufs[i]);
    // Performing death tests post-allocation times out on Windows.
  }
}
INSTANTIATE_TEST_SUITE_P(VaryNumPages,
                         GuardedPageAllocatorParamTest,
                         testing::Values(1, kMaxMetadata / 2, kMaxMetadata));

class ThreadedAllocCountDelegate : public base::DelegateSimpleThread::Delegate {
 public:
  ThreadedAllocCountDelegate(GuardedPageAllocator* gpa,
                             std::array<void*, kMaxMetadata>* allocations)
      : gpa_(gpa), allocations_(allocations) {}

  ThreadedAllocCountDelegate(const ThreadedAllocCountDelegate&) = delete;
  ThreadedAllocCountDelegate& operator=(const ThreadedAllocCountDelegate&) =
      delete;

  void Run() override {
    for (size_t i = 0; i < kMaxMetadata; i++) {
      (*allocations_)[i] = gpa_->Allocate(1);
    }
  }

 private:
  raw_ptr<GuardedPageAllocator> gpa_;
  raw_ptr<std::array<void*, kMaxMetadata>> allocations_;
};

// Test that no pages are double-allocated or left unallocated, and that no
// extra pages are allocated when there's concurrent calls to Allocate().
TEST_P(GuardedPageAllocatorTest, ThreadedAllocCount) {
  constexpr size_t num_threads = 2;
  std::array<void*, kMaxMetadata> allocations[num_threads];
  {
    base::DelegateSimpleThreadPool threads("alloc_threads", num_threads);
    threads.Start();

    std::vector<std::unique_ptr<ThreadedAllocCountDelegate>> delegates;
    for (size_t i = 0; i < num_threads; i++) {
      auto delegate =
          std::make_unique<ThreadedAllocCountDelegate>(&gpa_, &allocations[i]);
      threads.AddWork(delegate.get());
      delegates.push_back(std::move(delegate));
    }

    threads.JoinAll();
  }
  std::set<void*> allocations_set;
  for (size_t i = 0; i < num_threads; i++) {
    for (size_t j = 0; j < kMaxMetadata; j++) {
      allocations_set.insert(allocations[i][j]);
    }
  }
  allocations_set.erase(nullptr);
  EXPECT_EQ(allocations_set.size(), kMaxMetadata);
}

class ThreadedHighContentionDelegate
    : public base::DelegateSimpleThread::Delegate {
 public:
  explicit ThreadedHighContentionDelegate(GuardedPageAllocator* gpa)
      : gpa_(gpa) {}

  ThreadedHighContentionDelegate(const ThreadedHighContentionDelegate&) =
      delete;
  ThreadedHighContentionDelegate& operator=(
      const ThreadedHighContentionDelegate&) = delete;

  void Run() override {
    char* buf;
    while ((buf = reinterpret_cast<char*>(gpa_->Allocate(1))) == nullptr) {
      base::PlatformThread::Sleep(base::Nanoseconds(5000));
    }

    // Verify that no other thread has access to this page.
    EXPECT_EQ(buf[0], 0);

    // Mark this page and allow some time for another thread to potentially
    // gain access to this page.
    buf[0] = 'A';
    base::PlatformThread::Sleep(base::Nanoseconds(10000));
    EXPECT_EQ(buf[0], 'A');

    // Unmark this page and deallocate.
    buf[0] = 0;
    gpa_->Deallocate(buf);
  }

 private:
  raw_ptr<GuardedPageAllocator> gpa_;
};

// Test that allocator remains in consistent state under high contention and
// doesn't double-allocate pages or fail to deallocate pages.
TEST_P(GuardedPageAllocatorTest, ThreadedHighContention) {
#if BUILDFLAG(IS_ANDROID)
  constexpr size_t num_threads = 200;
#else
  constexpr size_t num_threads = 1000;
#endif
  {
    base::DelegateSimpleThreadPool threads("page_writers", num_threads);
    threads.Start();

    std::vector<std::unique_ptr<ThreadedHighContentionDelegate>> delegates;
    for (size_t i = 0; i < num_threads; i++) {
      auto delegate = std::make_unique<ThreadedHighContentionDelegate>(&gpa_);
      threads.AddWork(delegate.get());
      delegates.push_back(std::move(delegate));
    }

    threads.JoinAll();
  }

  // Verify all pages have been deallocated now that all threads are done.
  for (size_t i = 0; i < kMaxMetadata; i++)
    EXPECT_NE(gpa_.Allocate(1), nullptr);
}

class GuardedPageAllocatorPartitionAllocTest : public BaseGpaTest {
 protected:
  GuardedPageAllocatorPartitionAllocTest()
      : BaseGpaTest(kMaxMetadata, kMaxMetadata, kMaxSlots, true) {}
};

TEST_F(GuardedPageAllocatorPartitionAllocTest,
       DifferentPartitionsNeverOverlap) {
  constexpr const char* kType1 = "fake type1";
  constexpr const char* kType2 = "fake type2";

  std::set<void*> type1, type2;
  for (size_t i = 0; i < kMaxSlots * 3; i++) {
    void* alloc1 = gpa_.Allocate(1, 0, kType1);
    ASSERT_NE(alloc1, nullptr);
    void* alloc2 = gpa_.Allocate(1, 0, kType2);
    ASSERT_NE(alloc2, nullptr);

    type1.insert(alloc1);
    type2.insert(alloc2);

    gpa_.Deallocate(alloc1);
    gpa_.Deallocate(alloc2);
  }

  std::vector<void*> intersection;
  std::set_intersection(type1.begin(), type1.end(), type2.begin(), type2.end(),
                        std::back_inserter(intersection));

  EXPECT_EQ(intersection.size(), 0u);
}

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_GWP_ASAN_STORE)
constexpr size_t kSmallMaxSlots = kMaxMetadata;
class GuardedPageAllocatorRawPtrTest : public BaseGpaTest {
 protected:
  GuardedPageAllocatorRawPtrTest()
      // For these tests the number of available slots has to be equal to
      // the number of metadata entries. We don't want to end up in a
      // situation where an allocation attempt fails because there's nowhere to
      // store metadata while there are still available allocation slots.
      : BaseGpaTest(kSmallMaxSlots, kSmallMaxSlots, kSmallMaxSlots, false) {}
};

TEST_F(GuardedPageAllocatorRawPtrTest, DeferDeallocation) {
  for (size_t i = 0; i < kSmallMaxSlots - 1; i++)
    EXPECT_NE(gpa_.Allocate(1), nullptr);

  raw_ptr<void> ptr = gpa_.Allocate(1);
  gpa_.Deallocate(ptr);

  // Dangling raw_ptr should prevent the allocation from being reused.
  EXPECT_EQ(gpa_.Allocate(1), nullptr);

  ptr = nullptr;
  // Now we should get one slot back...
  EXPECT_NE(gpa_.Allocate(1), nullptr);
  // But just one.
  EXPECT_EQ(gpa_.Allocate(1), nullptr);
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_GWP_ASAN_STORE)

}  // namespace internal
}  // namespace gwp_asan
