// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/client/sampling_malloc_shims.h"

#include <stdlib.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/page_size.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/gwp_asan.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

// These tests install global allocator shims so they are not safe to run in
// multi-threaded contexts. Instead they're implemented as multi-process tests.

#if BUILDFLAG(IS_WIN)
#include <malloc.h>
static size_t GetUsableSize(void* mem) {
  return _msize(mem);
}
#elif BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
static size_t GetUsableSize(void* mem) {
  return malloc_size(mem);
}
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
static size_t GetUsableSize(void* mem) {
  return malloc_usable_size(mem);
}
#endif

namespace gwp_asan {
namespace internal {

extern GuardedPageAllocator& GetMallocGpaForTesting();

namespace {

constexpr size_t kSamplingFrequency = 5;
// Number of loop iterations required to hit a sampled allocation.
// The probability of not hitting a sample allocation in kLoopIterations
// is (1 - 1/kSamplingFrequency)^kLoopIterations. In this case that is
// (4/5)^100 < 3*10^-10.
constexpr size_t kLoopIterations = 100;

constexpr int kSuccess = 0;
constexpr int kFailure = 1;

class SamplingMallocShimsTest : public base::MultiProcessTest {
 public:
  static void multiprocessTestSetup() {
#if BUILDFLAG(IS_APPLE)
    allocator_shim::InitializeAllocatorShim();
#endif  // BUILDFLAG(IS_APPLE)
    crash_reporter::InitializeCrashKeys();
    InstallMallocHooks(
        AllocatorSettings{
            .max_allocated_pages = AllocatorState::kMaxMetadata,
            .num_metadata = AllocatorState::kMaxMetadata,
            .total_pages = AllocatorState::kMaxRequestedSlots,
            .sampling_frequency = kSamplingFrequency,
        },
        base::DoNothing());
  }

 protected:
  void runTest(const char* name) {
    base::Process process = SpawnChild(name);
    int exit_code = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, kSuccess);
  }
};

// Return whether some of the allocations returned by the calling the allocate
// parameter are sampled to the guarded allocator. Keep count of failures
// encountered.
bool allocationCheck(std::function<void*(void)> allocate,
                     std::function<void(void*)> free,
                     int* failures) {
  size_t guarded = 0;
  size_t unguarded = 0;
  for (size_t i = 0; i < kLoopIterations; i++) {
    std::unique_ptr<void, decltype(free)> alloc(allocate(), free);
    EXPECT_NE(alloc.get(), nullptr);
    if (!alloc.get()) {
      *failures += 1;
      return false;
    }

    if (GetMallocGpaForTesting().PointerIsMine(alloc.get()))
      guarded++;
    else
      unguarded++;
  }

  if (guarded > 0 && unguarded > 0)
    return true;

  *failures += 1;
  return false;
}

MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    BasicFunctionality,
    SamplingMallocShimsTest::multiprocessTestSetup) {
  const size_t page_size = base::GetPageSize();
  int failures = 0;

  EXPECT_TRUE(
      allocationCheck([&] { return malloc(page_size); }, &free, &failures));
  EXPECT_TRUE(
      allocationCheck([&] { return calloc(1, page_size); }, &free, &failures));
  EXPECT_TRUE(allocationCheck([&] { return realloc(nullptr, page_size); },
                              &free, &failures));

#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(allocationCheck([&] { return _aligned_malloc(123, 16); },
                              &_aligned_free, &failures));
  EXPECT_TRUE(
      allocationCheck([&] { return _aligned_realloc(nullptr, 123, 16); },
                      &_aligned_free, &failures));
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
  EXPECT_TRUE(allocationCheck(
      [&]() -> void* {
        void* ptr;
        if (posix_memalign(&ptr, page_size, page_size))
          return nullptr;
        return ptr;
      },
      &free, &failures));
#endif  // BUILDFLAG(IS_POSIX)

  EXPECT_TRUE(allocationCheck([&] { return std::malloc(page_size); },
                              &std::free, &failures));
  EXPECT_TRUE(allocationCheck([&] { return std::calloc(1, page_size); },
                              &std::free, &failures));
  EXPECT_TRUE(allocationCheck([&] { return std::realloc(nullptr, page_size); },
                              &std::free, &failures));

  EXPECT_TRUE(allocationCheck([] { return new int; },
                              [](void* ptr) { delete (int*)ptr; }, &failures));
  EXPECT_TRUE(allocationCheck([] { return new int[4]; },
                              [](void* ptr) { delete[](int*) ptr; },
                              &failures));

  if (failures)
    return kFailure;

  EXPECT_FALSE(
      allocationCheck([&] { return malloc(page_size + 1); }, &free, &failures));

  // Make sure exactly 1 negative test case was hit.
  if (failures == 1)
    return kSuccess;

  return kFailure;
}

// Flaky on Mac: https://crbug.com/1087372
#if BUILDFLAG(IS_APPLE)
#define MAYBE_BasicFunctionality DISABLED_BasicFunctionality
#else
#define MAYBE_BasicFunctionality BasicFunctionality
#endif
TEST_F(SamplingMallocShimsTest, MAYBE_BasicFunctionality) {
  runTest("BasicFunctionality");
}

MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    Realloc,
    SamplingMallocShimsTest::multiprocessTestSetup) {
  void* alloc = GetMallocGpaForTesting().Allocate(base::GetPageSize());
  CHECK_NE(alloc, nullptr);

  constexpr unsigned char kFillChar = 0xff;
  memset(alloc, kFillChar, base::GetPageSize());

  unsigned char* new_alloc =
      static_cast<unsigned char*>(realloc(alloc, base::GetPageSize() + 1));
  CHECK_NE(alloc, new_alloc);
  CHECK_EQ(GetMallocGpaForTesting().PointerIsMine(new_alloc), false);

  for (size_t i = 0; i < base::GetPageSize(); i++)
    CHECK_EQ(new_alloc[i], kFillChar);

  free(new_alloc);

  return kSuccess;
}

TEST_F(SamplingMallocShimsTest, Realloc) {
  runTest("Realloc");
}

MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    Calloc,
    SamplingMallocShimsTest::multiprocessTestSetup) {
  for (size_t i = 0; i < kLoopIterations; i++) {
    unsigned char* alloc =
        static_cast<unsigned char*>(calloc(base::GetPageSize(), 1));
    CHECK_NE(alloc, nullptr);

    if (GetMallocGpaForTesting().PointerIsMine(alloc)) {
      for (size_t j = 0; j < base::GetPageSize(); j++)
        CHECK_EQ(alloc[j], 0U);
      free(alloc);
      return kSuccess;
    }

    free(alloc);
  }

  return kFailure;
}

TEST_F(SamplingMallocShimsTest, Calloc) {
  runTest("Calloc");
}

// GetCrashKeyValue() operates on a per-component basis, can't read the crash
// key from the gwp_asan_client component in a component build.
#if !defined(COMPONENT_BUILD)
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    CrashKey,
    SamplingMallocShimsTest::multiprocessTestSetup) {
  if (crash_reporter::GetCrashKeyValue(kMallocCrashKey) !=
      GetMallocGpaForTesting().GetCrashKey()) {
    return kFailure;
  }

  return kSuccess;
}

TEST_F(SamplingMallocShimsTest, CrashKey) {
  runTest("CrashKey");
}
#endif  // !defined(COMPONENT_BUILD)

// malloc_usable_size() is not currently used/shimmed on Android.
#if !BUILDFLAG(IS_ANDROID)
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    GetSizeEstimate,
    SamplingMallocShimsTest::multiprocessTestSetup) {
  constexpr size_t kAllocationSize = 123;
  for (size_t i = 0; i < kLoopIterations; i++) {
    std::unique_ptr<void, decltype(&free)> alloc(malloc(kAllocationSize), free);
    CHECK_NE(alloc.get(), nullptr);

    size_t alloc_sz = GetUsableSize(alloc.get());
    if (GetMallocGpaForTesting().PointerIsMine(alloc.get()))
      CHECK_EQ(alloc_sz, kAllocationSize);
    else
      CHECK_GE(alloc_sz, kAllocationSize);
  }

  return kSuccess;
}

TEST_F(SamplingMallocShimsTest, GetSizeEstimate) {
  runTest("GetSizeEstimate");
}
#endif

#if BUILDFLAG(IS_WIN)
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    AlignedRealloc,
    SamplingMallocShimsTest::multiprocessTestSetup) {
  // Exercise the _aligned_* shims and ensure that we handle them stably.
  constexpr size_t kAllocationSize = 123;
  constexpr size_t kAllocationAlignment = 64;
  for (size_t i = 0; i < kLoopIterations; i++) {
    void* ptr = _aligned_malloc(kAllocationSize, kAllocationAlignment);
    CHECK(ptr);
    ptr = _aligned_realloc(ptr, kAllocationSize * 2, kAllocationAlignment);
    CHECK(ptr);
    _aligned_free(ptr);
  }

  return kSuccess;
}

TEST_F(SamplingMallocShimsTest, AlignedRealloc) {
  runTest("AlignedRealloc");
}
#endif  // BUILDFLAG(IS_WIN)

// PartitionAlloc-Everywhere does not support batch_malloc / batch_free.
#if BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    BatchFree,
    SamplingMallocShimsTest::multiprocessTestSetup) {
  void* ptrs[AllocatorState::kMaxMetadata + 1];
  for (size_t i = 0; i < AllocatorState::kMaxMetadata; i++) {
    ptrs[i] = GetMallocGpaForTesting().Allocate(16);
    CHECK(ptrs[i]);
  }
  // Check that all GPA allocations were consumed.
  CHECK_EQ(GetMallocGpaForTesting().Allocate(16), nullptr);

  ptrs[AllocatorState::kMaxMetadata] =
      malloc_zone_malloc(malloc_default_zone(), 16);
  CHECK(ptrs[AllocatorState::kMaxMetadata]);

  malloc_zone_batch_free(malloc_default_zone(), ptrs,
                         AllocatorState::kMaxMetadata + 1);

  // Check that GPA allocations were freed.
  CHECK(GetMallocGpaForTesting().Allocate(16));

  return kSuccess;
}

TEST_F(SamplingMallocShimsTest, BatchFree) {
  runTest("BatchFree");
}
#endif  // BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace

}  // namespace internal
}  // namespace gwp_asan
