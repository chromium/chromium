// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.h"

#include <string>
#include <vector>

#include "base/rand_util.h"
#include "build/build_config.h"
#include "chromeos/ash/components/memory/userspace_swap/region.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_constants.h"
#endif

namespace ash {
namespace memory {
namespace userspace_swap {

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_BUILDFLAG(HAS_64_BIT_POINTERS)
// InRange matches if the range specified by [start,end] is in [address,
// address+length] of a region.
MATCHER_P2(InRange, start, end, "") {
  return start >= arg->address && end <= (arg->address + arg->length);
}

TEST(UserspaceSwap, GetUsedSuperpages) {
  // Allocate 1000 different memory areas between 128 bytes and 20 superpages in
  // size.
  constexpr size_t kNumAllocations = 1000;
  constexpr size_t kMaxAllocationSize = 2 * partition_alloc::kSuperPageSize;
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
  constexpr size_t kMaxAllocationSize = 5 * partition_alloc::kSuperPageSize;
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
  ASSERT_LE(total_length, 5 * partition_alloc::kSuperPageSize);

  // Cleanup
  for (size_t i = 0; i < kNumAllocations; ++i) {
    free(reinterpret_cast<void*>(mem_area[i]));
  }
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
