// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/client/sampling_malloc_shims.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "partition_alloc/shim/allocator_shim.h"

#if BUILDFLAG(IS_APPLE)
#include <pthread.h>
#endif

namespace gwp_asan {
namespace internal {

namespace {

using allocator_shim::AllocatorDispatch;

// By being implemented as a global with inline method definitions, method calls
// and member accesses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
//
// Note that this optimization has not been benchmarked. However since it is
// easy to do there is no reason to pay the extra cost.
SamplingState<MALLOC> sampling_state;

// The global allocator singleton used by the shims. Implemented as a global
// pointer instead of a function-local static to avoid initialization checks
// for every access.
GuardedPageAllocator* gpa = nullptr;

extern AllocatorDispatch g_allocator_dispatch;

void* AllocFn(size_t size, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (void* allocation = gpa->Allocate(size))
      return allocation;
  }

  return g_allocator_dispatch.next->alloc_function(size, context);
}

void* AllocUncheckedFn(size_t size, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (void* allocation = gpa->Allocate(size))
      return allocation;
  }

  return g_allocator_dispatch.next->alloc_unchecked_function(size, context);
}

void* AllocZeroInitializedFn(size_t n, size_t size, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    base::CheckedNumeric<size_t> checked_total = size;
    checked_total *= n;
    if (!checked_total.IsValid()) [[unlikely]] {
      return nullptr;
    }

    size_t total_size = checked_total.ValueOrDie();
    if (void* allocation = gpa->Allocate(total_size)) {
      memset(allocation, 0, total_size);
      return allocation;
    }
  }

  return g_allocator_dispatch.next->alloc_zero_initialized_function(n, size,
                                                                    context);
}

void* AllocAlignedFn(size_t alignment, size_t size, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (void* allocation = gpa->Allocate(size, alignment))
      return allocation;
  }

  return g_allocator_dispatch.next->alloc_aligned_function(alignment, size,
                                                           context);
}

void* ReallocFn(void* address, size_t size, void* context) {
  if (!address) [[unlikely]] {
    return AllocFn(size, context);
  }

  if (!gpa->PointerIsMine(address)) [[likely]] {
    return g_allocator_dispatch.next->realloc_function(address, size, context);
  }

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size);
  if (!new_alloc)
    new_alloc = g_allocator_dispatch.next->alloc_function(size, context);
  if (!new_alloc)
    return nullptr;

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

void* ReallocUncheckedFn(void* address, size_t size, void* context) {
  if (!address) [[unlikely]] {
    return AllocFn(size, context);
  }

  if (!gpa->PointerIsMine(address)) [[likely]] {
    return g_allocator_dispatch.next->realloc_unchecked_function(address, size,
                                                                 context);
  }

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size);
  if (!new_alloc) {
    new_alloc =
        g_allocator_dispatch.next->alloc_unchecked_function(size, context);
  }
  if (!new_alloc) {
    return nullptr;
  }

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

void FreeFn(void* address, void* context) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    return gpa->Deallocate(address);
  }

  g_allocator_dispatch.next->free_function(address, context);
}

size_t GetSizeEstimateFn(void* address, void* context) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    return gpa->GetRequestedSize(address);
  }

  return g_allocator_dispatch.next->get_size_estimate_function(address,
                                                               context);
}

size_t GoodSizeFn(size_t size, void* context) {
  // We don't know whether the allocation would be handled by the guarded page
  // allocator, cannot return what it would prefer here.
  return g_allocator_dispatch.next->good_size_function(size, context);
}

bool ClaimedAddressFn(void* address, void* context) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    return true;
  }

  return g_allocator_dispatch.next->claimed_address_function(address, context);
}

unsigned BatchMallocFn(size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  // The batch_malloc() routine is esoteric and only accessible for the system
  // allocator's zone, GWP-ASan interception is not provided.

  return g_allocator_dispatch.next->batch_malloc_function(
      size, results, num_requested, context);
}

void BatchFreeFn(void** to_be_freed, unsigned num_to_be_freed, void* context) {
  // A batch_free() hook is implemented because it is imperative that we never
  // call free() with a GWP-ASan allocation.
  for (size_t i = 0; i < num_to_be_freed; i++) {
    if (gpa->PointerIsMine(to_be_freed[i])) [[unlikely]] {
      // If this batch includes guarded allocations, call free() on all of the
      // individual allocations to ensure the guarded allocations are handled
      // correctly.
      for (size_t j = 0; j < num_to_be_freed; j++)
        FreeFn(to_be_freed[j], context);
      return;
    }
  }

  g_allocator_dispatch.next->batch_free_function(to_be_freed, num_to_be_freed,
                                                 context);
}

void FreeDefiniteSizeFn(void* address, size_t size, void* context) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    // TODO(vtsyrklevich): Perform this check in GuardedPageAllocator and report
    // failed checks using the same pipeline.
    CHECK_EQ(size, gpa->GetRequestedSize(address));
    gpa->Deallocate(address);
    return;
  }

  g_allocator_dispatch.next->free_definite_size_function(address, size,
                                                         context);
}

void TryFreeDefaultFn(void* address, void* context) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    gpa->Deallocate(address);
    return;
  }

  g_allocator_dispatch.next->try_free_default_function(address, context);
}

static void* AlignedMallocFn(size_t size, size_t alignment, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (void* allocation = gpa->Allocate(size, alignment))
      return allocation;
  }

  return g_allocator_dispatch.next->aligned_malloc_function(size, alignment,
                                                            context);
}

static void* AlignedMallocUncheckedFn(size_t size,
                                      size_t alignment,
                                      void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (void* allocation = gpa->Allocate(size, alignment)) {
      return allocation;
    }
  }

  return g_allocator_dispatch.next->aligned_malloc_unchecked_function(
      size, alignment, context);
}

static void* AlignedReallocFn(void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  if (!address) [[unlikely]] {
    return AlignedMallocFn(size, alignment, context);
  }

  if (!gpa->PointerIsMine(address)) [[likely]] {
    return g_allocator_dispatch.next->aligned_realloc_function(
        address, size, alignment, context);
  }

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size, alignment);
  if (!new_alloc)
    new_alloc = g_allocator_dispatch.next->aligned_malloc_function(
        size, alignment, context);
  if (!new_alloc)
    return nullptr;

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

static void* AlignedReallocUncheckedFn(void* address,
                                       size_t size,
                                       size_t alignment,
                                       void* context) {
  if (!address) [[unlikely]] {
    return AlignedMallocFn(size, alignment, context);
  }

  if (!gpa->PointerIsMine(address)) [[likely]] {
    return g_allocator_dispatch.next->aligned_realloc_unchecked_function(
        address, size, alignment, context);
  }

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size, alignment);
  if (!new_alloc) {
    new_alloc = g_allocator_dispatch.next->aligned_malloc_unchecked_function(
        size, alignment, context);
  }
  if (!new_alloc) {
    return nullptr;
  }

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

static void AlignedFreeFn(void* address, void* context) {
  if (gpa->PointerIsMine(address)) [[unlikely]] {
    return gpa->Deallocate(address);
  }

  g_allocator_dispatch.next->aligned_free_function(address, context);
}

AllocatorDispatch g_allocator_dispatch = {
    &AllocFn,
    &AllocUncheckedFn,
    &AllocZeroInitializedFn,
    &AllocAlignedFn,
    &ReallocFn,
    &ReallocUncheckedFn,
    &FreeFn,
    &GetSizeEstimateFn,
    &GoodSizeFn,
    &ClaimedAddressFn,
    &BatchMallocFn,
    &BatchFreeFn,
    &FreeDefiniteSizeFn,
    &TryFreeDefaultFn,
    &AlignedMallocFn,
    &AlignedMallocUncheckedFn,
    &AlignedReallocFn,
    &AlignedReallocUncheckedFn,
    &AlignedFreeFn,
    nullptr /* next */
};

}  // namespace

// We expose the allocator singleton for unit tests.
GWP_ASAN_EXPORT GuardedPageAllocator& GetMallocGpaForTesting() {
  return *gpa;
}

void InstallMallocHooks(const AllocatorSettings& settings,
                        GuardedPageAllocator::OutOfMemoryCallback callback) {
  static crash_reporter::CrashKeyString<24> malloc_crash_key(kMallocCrashKey);
  gpa = new GuardedPageAllocator();
  gpa->Init(settings, std::move(callback), false);
  malloc_crash_key.Set(gpa->GetCrashKey());
  sampling_state.Init(settings.sampling_frequency);
  allocator_shim::InsertAllocatorDispatch(&g_allocator_dispatch);
}

}  // namespace internal

bool IsGwpAsanMallocAllocation(const void* ptr) {
  return internal::gpa && internal::gpa->PointerIsMine(ptr);
}

}  // namespace gwp_asan
