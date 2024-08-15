// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/malloc_shims.h"

#include <limits>
#include <optional>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "partition_alloc/shim/allocator_shim.h"

namespace gwp_asan::internal::lud {

namespace {

using allocator_shim::AllocatorDispatch;

extern AllocatorDispatch g_allocator_dispatch;

// By being implemented as a global with inline method definitions, method calls
// and member accesses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
SamplingState<LIGHTWEIGHTDETECTOR> sampling_state;

bool MaybeQuarantine(void* address,
                     std::optional<size_t> maybe_size,
                     void* context,
                     FreeFunctionKind kind) {
  if (!sampling_state.Sample()) [[likely]] {
    return false;
  }

  AllocationInfo info;
  info.address = address;
#if BUILDFLAG(IS_APPLE)
  info.context = context;
#else
  DCHECK_EQ(context, nullptr);
#endif
  base::CheckedNumeric<size_t> size = maybe_size.value_or(
      g_allocator_dispatch.next->get_size_estimate_function(address, context));
  info.free_fn_kind = kind;
  if (!size.AssignIfValid(&info.size)) [[unlikely]] {
    return false;
  }

  return RandomEvictionQuarantine::Get()->Add(info);
}

void FreeFn(void* address, void* context) {
  if (MaybeQuarantine(address, std::nullopt, context,
                      FreeFunctionKind::kFree)) {
    return;
  }

  MUSTTAIL return g_allocator_dispatch.next->free_function(address, context);
}

void FreeDefiniteSizeFn(void* address, size_t size, void* context) {
  if (MaybeQuarantine(address, size, context,
                      FreeFunctionKind::kFreeDefiniteSize)) {
    return;
  }

  MUSTTAIL return g_allocator_dispatch.next->free_definite_size_function(
      address, size, context);
}

void TryFreeDefaultFn(void* address, void* context) {
  if (MaybeQuarantine(address, std::nullopt, context,
                      FreeFunctionKind::kTryFreeDefault)) {
    return;
  }

  MUSTTAIL return g_allocator_dispatch.next->try_free_default_function(address,
                                                                       context);
}

static void AlignedFreeFn(void* address, void* context) {
  if (MaybeQuarantine(address, std::nullopt, context,
                      FreeFunctionKind::kAlignedFree)) {
    return;
  }

  MUSTTAIL return g_allocator_dispatch.next->aligned_free_function(address,
                                                                   context);
}

AllocatorDispatch g_allocator_dispatch = {
    nullptr,             // alloc_function
    nullptr,             // alloc_unchecked_function
    nullptr,             // alloc_zero_initialized_function
    nullptr,             // alloc_aligned_function
    nullptr,             // realloc_function
    nullptr,             // realloc_unchecked_function
    FreeFn,              // free_function
    nullptr,             // get_size_estimate_function
    nullptr,             // good_size_function
    nullptr,             // claimed_address_function
    nullptr,             // batch_malloc_function
    nullptr,             // batch_free_function
    FreeDefiniteSizeFn,  // free_definite_size_function
    TryFreeDefaultFn,    // try_free_default_function
    nullptr,             // aligned_malloc_function
    nullptr,             // aligned_malloc_unchecked_function
    nullptr,             // aligned_realloc_function
    nullptr,             // aligned_realloc_unchecked_function
    AlignedFreeFn,       // aligned_free_function
    nullptr              // next
};

}  // namespace

void InstallMallocHooks(size_t max_allocation_count,
                        size_t max_total_size,
                        size_t total_size_high_water_mark,
                        size_t total_size_low_water_mark,
                        size_t eviction_chunk_size,
                        size_t eviction_task_interval_ms,
                        size_t sampling_frequency) {
  sampling_state.Init(sampling_frequency);
  RandomEvictionQuarantine::Init(max_allocation_count, max_total_size,
                                 total_size_high_water_mark,
                                 total_size_low_water_mark, eviction_chunk_size,
                                 eviction_task_interval_ms);
  allocator_shim::InsertAllocatorDispatch(&g_allocator_dispatch);
}

void FinishFree(const AllocationInfo& allocation) {
#if BUILDFLAG(IS_APPLE)
  void* context = allocation.context;
#else
  void* context = nullptr;
#endif

  const AllocatorDispatch* next = g_allocator_dispatch.next;

  switch (allocation.free_fn_kind) {
    case FreeFunctionKind::kFree:
      next->free_function(allocation.address, context);
      break;
    case FreeFunctionKind::kFreeDefiniteSize:
      next->free_definite_size_function(allocation.address, allocation.size,
                                        context);
      break;
    case FreeFunctionKind::kTryFreeDefault:
      next->try_free_default_function(allocation.address, context);
      break;
    case FreeFunctionKind::kAlignedFree:
      next->aligned_free_function(allocation.address, context);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace gwp_asan::internal::lud
