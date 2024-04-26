// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/malloc_shims.h"

#include <limits>
#include <optional>

#include "base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"
#include "components/gwp_asan/client/sampling_state.h"

namespace gwp_asan::internal::lud {

namespace {

using allocator_shim::AllocatorDispatch;

// By being implemented as a global with inline method definitions, method calls
// and member accesses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
SamplingState<LIGHTWEIGHTDETECTOR> sampling_state;

bool MaybeQuarantine(const AllocatorDispatch* self,
                     void* address,
                     std::optional<size_t> maybe_size,
                     void* context,
                     FreeFunctionKind kind) {
  if (LIKELY(!sampling_state.Sample())) {
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
      self->next->get_size_estimate_function(self->next, address, context));
  info.free_fn_kind = kind;
  if (UNLIKELY(!size.AssignIfValid(&info.size))) {
    return false;
  }

  return RandomEvictionQuarantine::Get()->Add(info);
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  if (MaybeQuarantine(self, address, std::nullopt, context,
                      FreeFunctionKind::kFree)) {
    return;
  }

  self->next->free_function(self->next, address, context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  if (MaybeQuarantine(self, address, size, context,
                      FreeFunctionKind::kFreeDefiniteSize)) {
    return;
  }

  self->next->free_definite_size_function(self->next, address, size, context);
}

void TryFreeDefaultFn(const AllocatorDispatch* self,
                      void* address,
                      void* context) {
  if (MaybeQuarantine(self, address, std::nullopt, context,
                      FreeFunctionKind::kTryFreeDefault)) {
    return;
  }

  self->next->try_free_default_function(self->next, address, context);
}

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {
  if (MaybeQuarantine(self, address, std::nullopt, context,
                      FreeFunctionKind::kAlignedFree)) {
    return;
  }

  self->next->aligned_free_function(self->next, address, context);
}

AllocatorDispatch g_allocator_dispatch = {
    nullptr,             // alloc_function
    nullptr,             // alloc_unchecked_function
    nullptr,             // alloc_zero_initialized_function
    nullptr,             // alloc_aligned_function
    nullptr,             // realloc_function
    FreeFn,              // free_function
    nullptr,             // get_size_estimate_function
    nullptr,             // good_size_function
    nullptr,             // claimed_address_function
    nullptr,             // batch_malloc_function
    nullptr,             // batch_free_function
    FreeDefiniteSizeFn,  // free_definite_size_function
    TryFreeDefaultFn,    // try_free_default_function
    nullptr,             // aligned_malloc_function
    nullptr,             // aligned_realloc_function
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
      next->free_function(next, allocation.address, context);
      break;
    case FreeFunctionKind::kFreeDefiniteSize:
      next->free_definite_size_function(next, allocation.address,
                                        allocation.size, context);
      break;
    case FreeFunctionKind::kTryFreeDefault:
      next->try_free_default_function(next, allocation.address, context);
      break;
    case FreeFunctionKind::kAlignedFree:
      next->aligned_free_function(next, allocation.address, context);
      break;
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace gwp_asan::internal::lud
