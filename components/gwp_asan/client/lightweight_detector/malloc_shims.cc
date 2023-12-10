// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/malloc_shims.h"

#include <limits>

#include "base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gwp_asan::internal::lud {

namespace {
using allocator_shim::AllocatorDispatch;

// By being implemented as a global with inline method definitions, method calls
// and member accesses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
SamplingState<LIGHTWEIGHTDETECTOR> sampling_state;

bool MaybeQuarantine(const AllocatorDispatch* self,
                     void* address,
                     absl::optional<size_t> maybe_size,
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

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  return self->next->alloc_function(self->next, size, context);
}

void* AllocUncheckedFn(const AllocatorDispatch* self,
                       size_t size,
                       void* context) {
  return self->next->alloc_unchecked_function(self->next, size, context);
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  return self->next->alloc_zero_initialized_function(self->next, n, size,
                                                     context);
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  return self->next->alloc_aligned_function(self->next, alignment, size,
                                            context);
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  return self->next->realloc_function(self->next, address, size, context);
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  if (MaybeQuarantine(self, address, absl::nullopt, context,
                      FreeFunctionKind::kFree)) {
    return;
  }

  self->next->free_function(self->next, address, context);
}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  return self->next->get_size_estimate_function(self->next, address, context);
}

size_t GoodSizeFn(const AllocatorDispatch* self, size_t size, void* context) {
  return self->next->good_size_function(self->next, size, context);
}

bool ClaimedAddressFn(const AllocatorDispatch* self,
                      void* address,
                      void* context) {
  return self->next->claimed_address_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  return self->next->batch_malloc_function(self->next, size, results,
                                           num_requested, context);
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
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
  if (MaybeQuarantine(self, address, absl::nullopt, context,
                      FreeFunctionKind::kTryFreeDefault)) {
    return;
  }

  self->next->try_free_default_function(self->next, address, context);
}

static void* AlignedMallocFn(const AllocatorDispatch* self,
                             size_t size,
                             size_t alignment,
                             void* context) {
  return self->next->aligned_malloc_function(self->next, size, alignment,
                                             context);
}

static void* AlignedReallocFn(const AllocatorDispatch* self,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  return self->next->aligned_realloc_function(self->next, address, size,
                                              alignment, context);
}

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {
  if (MaybeQuarantine(self, address, absl::nullopt, context,
                      FreeFunctionKind::kAlignedFree)) {
    return;
  }

  self->next->aligned_free_function(self->next, address, context);
}

AllocatorDispatch g_allocator_dispatch = {
    &AllocFn,
    &AllocUncheckedFn,
    &AllocZeroInitializedFn,
    &AllocAlignedFn,
    &ReallocFn,
    &FreeFn,
    &GetSizeEstimateFn,
    &GoodSizeFn,
    &ClaimedAddressFn,
    &BatchMallocFn,
    &BatchFreeFn,
    &FreeDefiniteSizeFn,
    &TryFreeDefaultFn,
    &AlignedMallocFn,
    &AlignedReallocFn,
    &AlignedFreeFn,
    nullptr /* next */
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
