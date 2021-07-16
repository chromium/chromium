// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/userspace_swap/userspace_swap.h"

#include <string>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chromeos/memory/userspace_swap/region.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#endif

namespace chromeos {
namespace memory {
namespace userspace_swap {

namespace {
using memory_instrumentation::mojom::VmRegion;
using memory_instrumentation::mojom::VmRegionPtr;

// DefaultEligibleRegion is a region which is eligible by default which can be
// made ineligible by changing one or more of the properties.
VmRegion DefaultEligibleRegion() {
  VmRegion r;
  r.start_address = 0xF00F00BA4;
  r.size_in_bytes =
      UserspaceSwapConfig::Get().vma_region_minimum_size_bytes + 1;
  r.protection_flags =
      VmRegion::kProtectionFlagsRead | VmRegion::kProtectionFlagsWrite;

  // These aren't necessary but we list them to be explicit about the
  // requirements.
  r.byte_locked = 0;
  r.mapped_file = std::string();

  return r;
}

}  // namespace

TEST(EligibleVMA, DefaultIsEligible) {
  ASSERT_TRUE(IsVMASwapEligible(DefaultEligibleRegion().Clone()));
}

TEST(EligibleVMA, SharedIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags |= VmRegion::kProtectionFlagsMayshare;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, ProtNoneIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags = 0;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, WrOnlyIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags = VmRegion::kProtectionFlagsWrite;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, RdOnlyIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags = VmRegion::kProtectionFlagsRead;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, ExecIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags |= VmRegion::kProtectionFlagsExec;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, FileBackedIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.mapped_file = "/some/file/foo.so";

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, AnyLockedRegionIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.byte_locked = 20 << 10;  // Any non-zero locked will do.
  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, RegionTooSmallIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.size_in_bytes =
      UserspaceSwapConfig::Get().vma_region_minimum_size_bytes - 1;
  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, RegionTooLargeIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.size_in_bytes =
      UserspaceSwapConfig::Get().vma_region_maximum_size_bytes + 1;
  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

// With PartitionAlloc as malloc(), there are no large enough ranges by default.
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
TEST(GetAllSwapEligibleVMAs, SimpleVerification) {
  std::vector<Region> regions;
  ASSERT_TRUE(GetAllSwapEligibleVMAs(getpid(), &regions));
  ASSERT_GT(regions.size(), 0u);
}
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_HAS_64_BITS_POINTERS)
// InRange matches if the range specified by [start,end] is in [address,
// address+length] of a region.
MATCHER_P2(InRange, start, end, "") {
  return start >= arg->address && end <= (arg->address + arg->length);
}

TEST(UserspaceSwap, GetUsedSuperpages) {
  // Allocate 1000 different memory areas between 128 bytes and 20 superpages in
  // size.
  constexpr size_t kNumAllocations = 1000;
  constexpr size_t kMaxAllocationSize = 2 * base::kSuperPageSize;
  constexpr size_t kMinAllocationSize = 128;

  uintptr_t mem_area[kNumAllocations] = {};
  uint64_t mem_area_len[kNumAllocations] = {};
  for (size_t i = 0; i < kNumAllocations; ++i) {
    mem_area_len[i] = base::RandInt(kMinAllocationSize, kMaxAllocationSize);
    mem_area[i] = reinterpret_cast<uintptr_t>(malloc(mem_area_len[i]));
    ASSERT_NE(mem_area[i], 0u);
  }

  // And we should expect to find all of our allocations.
  std::vector<::userspace_swap::mojom::MemoryRegionPtr> regions;
  ASSERT_TRUE(GetPartitionAllocSuperPagesInUse(-1, regions));
  for (size_t i = 0; i < kNumAllocations; ++i) {
    EXPECT_THAT(regions, testing::Contains(userspace_swap::InRange(
                             mem_area[i], mem_area[i] + mem_area_len[i])));
  }

  // Cleanup
  for (size_t i = 0; i < kNumAllocations; ++i) {
    free(reinterpret_cast<void*>(mem_area[i]));
  }
}

TEST(UserspaceSwap, LimitSuperpagesReturned) {
  // Allocate 1000 different memory areas.
  constexpr size_t kNumAllocations = 50;
  constexpr size_t kMaxAllocationSize = 5 * base::kSuperPageSize;
  constexpr size_t kMinAllocationSize = 1024;

  uintptr_t mem_area[kNumAllocations] = {};
  uint64_t mem_area_len[kNumAllocations] = {};
  for (size_t i = 0; i < kNumAllocations; ++i) {
    mem_area_len[i] = base::RandInt(kMinAllocationSize, kMaxAllocationSize);
    mem_area[i] = reinterpret_cast<uintptr_t>(malloc(mem_area_len[i]));
    ASSERT_NE(mem_area[i], 0u);
  }

  // All that will be returned is 5 superpages worth of in use memory.
  std::vector<::userspace_swap::mojom::MemoryRegionPtr> regions;
  ASSERT_TRUE(GetPartitionAllocSuperPagesInUse(5, regions));

  // Now if we sum up the length of the regions returned it should be less than
  // or equal to 5 superpages.
  uint64_t total_length = 0;
  for (size_t i = 0; i < regions.size(); ++i) {
    total_length += regions[i]->length;
  }
  ASSERT_LE(total_length, 5 * base::kSuperPageSize);

  // Cleanup
  for (size_t i = 0; i < kNumAllocations; ++i) {
    free(reinterpret_cast<void*>(mem_area[i]));
  }
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace userspace_swap
}  // namespace memory
}  // namespace chromeos
