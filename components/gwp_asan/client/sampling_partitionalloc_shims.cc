// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_partitionalloc_shims.h"

#include <algorithm>
#include <utility>

#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "partition_alloc/flags.h"
#include "partition_alloc/partition_alloc.h"

namespace gwp_asan {
namespace internal {

namespace {

SamplingState<PARTITIONALLOC> sampling_state;

// The global allocator singleton used by the shims. Implemented as a global
// pointer instead of a function-local static to avoid initialization checks
// for every access.
GuardedPageAllocator* gpa = nullptr;

bool AllocationHook(void** out,
                    partition_alloc::AllocFlags flags,
                    size_t size,
                    const char* type_name) {
  if (sampling_state.Sample()) [[unlikely]] {
    // Ignore allocation requests with unknown flags.
    // TODO(crbug.com/40277643): Add support for memory tagging in GWP-Asan.
    constexpr auto kKnownFlags = partition_alloc::AllocFlags::kReturnNull |
                                 partition_alloc::AllocFlags::kZeroFill;
    if (!ContainsFlags(kKnownFlags, flags)) {
      // Skip if |flags| is not a subset of |kKnownFlags|.
      // i.e. if we find an unknown flag.
      return false;
    }

    if (void* allocation = gpa->Allocate(size, 0, type_name)) {
      *out = allocation;
      return true;
    }
  }
  return false;
}

bool FreeHook(void* address) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    gpa->Deallocate(address);
    return true;
  }
  return false;
}

bool ReallocHook(size_t* out, void* address) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    *out = gpa->GetRequestedSize(address);
    return true;
  }
  return false;
}

}  // namespace

// We expose the allocator singleton for unit tests.
GWP_ASAN_EXPORT GuardedPageAllocator& GetPartitionAllocGpaForTesting() {
  return *gpa;
}

void InstallPartitionAllocHooks(
    const AllocatorSettings& settings,
    GuardedPageAllocator::OutOfMemoryCallback callback) {
  static crash_reporter::CrashKeyString<24> pa_crash_key(
      kPartitionAllocCrashKey);
  gpa = new GuardedPageAllocator();
  gpa->Init(settings, std::move(callback), true);
  pa_crash_key.Set(gpa->GetCrashKey());
  sampling_state.Init(settings.sampling_frequency);
  // TODO(vtsyrklevich): Allow SetOverrideHooks to be passed in so we can hook
  // PDFium's PartitionAlloc fork.
  partition_alloc::PartitionAllocHooks::SetOverrideHooks(
      &AllocationHook, &FreeHook, &ReallocHook);
}

}  // namespace internal
}  // namespace gwp_asan
