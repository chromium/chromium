// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_malloc_shims.h"

#include <algorithm>
#include <utility>

#include "base/allocator/allocator_shim.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "components/gwp_asan/common/crash_key_name.h"

#if defined(OS_MACOSX)
#include <pthread.h>
#endif

namespace gwp_asan {
namespace internal {

namespace {

using base::allocator::AllocatorDispatch;

// By being implemented as a global with inline method definitions, method calls
// and member acceses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
//
// Note that this optimization has not been benchmarked. However since it is
// easy to do there is no reason to pay the extra cost.
SamplingState<MALLOC> sampling_state;

// The global allocator singleton used by the shims. Implemented as a global
// pointer instead of a function-local static to avoid initialization checks
// for every access.
GuardedPageAllocator* gpa = nullptr;

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  if (UNLIKELY(sampling_state.Sample()))
    if (void* allocation = gpa->Allocate(size))
      return allocation;

  return self->next->alloc_function(self->next, size, context);
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  if (UNLIKELY(sampling_state.Sample())) {
    base::CheckedNumeric<size_t> checked_total = size;
    checked_total *= n;
    if (UNLIKELY(!checked_total.IsValid()))
      return nullptr;

    size_t total_size = checked_total.ValueOrDie();
    if (void* allocation = gpa->Allocate(total_size)) {
      memset(allocation, 0, total_size);
      return allocation;
    }
  }

  return self->next->alloc_zero_initialized_function(self->next, n, size,
                                                     context);
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  if (UNLIKELY(sampling_state.Sample()))
    if (void* allocation = gpa->Allocate(size, alignment))
      return allocation;

  return self->next->alloc_aligned_function(self->next, alignment, size,
                                            context);
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  if (UNLIKELY(!address))
    return AllocFn(self, size, context);

  if (LIKELY(!gpa->PointerIsMine(address)))
    return self->next->realloc_function(self->next, address, size, context);

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size);
  if (!new_alloc)
    new_alloc = self->next->alloc_function(self->next, size, context);
  if (!new_alloc)
    return nullptr;

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address)))
    return gpa->Deallocate(address);

  self->next->free_function(self->next, address, context);
}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address)))
    return gpa->GetRequestedSize(address);

  return self->next->get_size_estimate_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  // The batch_malloc() routine is esoteric and only accessible for the system
  // allocator's zone, GWP-ASan interception is not provided.

  return self->next->batch_malloc_function(self->next, size, results,
                                           num_requested, context);
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  // A batch_free() hook is implemented because it is imperative that we never
  // call free() with a GWP-ASan allocation.
  for (size_t i = 0; i < num_to_be_freed; i++) {
    if (UNLIKELY(gpa->PointerIsMine(to_be_freed[i]))) {
      // If this batch includes guarded allocations, call free() on all of the
      // individual allocations to ensure the guarded allocations are handled
      // correctly.
      for (size_t j = 0; j < num_to_be_freed; j++)
        FreeFn(self, to_be_freed[j], context);
      return;
    }
  }

  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address))) {
    // TODO(vtsyrklevich): Perform this check in GuardedPageAllocator and report
    // failed checks using the same pipeline.
    CHECK_EQ(size, gpa->GetRequestedSize(address));
    gpa->Deallocate(address);
    return;
  }

  self->next->free_definite_size_function(self->next, address, size, context);
}

static void* AlignedMallocFn(const AllocatorDispatch* self,
                             size_t size,
                             size_t alignment,
                             void* context) {
  if (UNLIKELY(sampling_state.Sample()))
    if (void* allocation = gpa->Allocate(size, alignment))
      return allocation;

  return self->next->aligned_malloc_function(self->next, size, alignment,
                                             context);
}

static void* AlignedReallocFn(const AllocatorDispatch* self,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  if (UNLIKELY(!address))
    return AlignedMallocFn(self, size, alignment, context);

  if (LIKELY(!gpa->PointerIsMine(address)))
    return self->next->aligned_realloc_function(self->next, address, size,
                                                alignment, context);

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size, alignment);
  if (!new_alloc)
    new_alloc = self->next->aligned_malloc_function(self->next, size, alignment,
                                                    context);
  if (!new_alloc)
    return nullptr;

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address)))
    return gpa->Deallocate(address);

  self->next->aligned_free_function(self->next, address, context);
}

AllocatorDispatch g_allocator_dispatch = {
    &AllocFn,
    &AllocZeroInitializedFn,
    &AllocAlignedFn,
    &ReallocFn,
    &FreeFn,
    &GetSizeEstimateFn,
    &BatchMallocFn,
    &BatchFreeFn,
    &FreeDefiniteSizeFn,
    &AlignedMallocFn,
    &AlignedReallocFn,
    &AlignedFreeFn,
    nullptr /* next */
};

}  // namespace

// We expose the allocator singleton for unit tests.
GWP_ASAN_EXPORT GuardedPageAllocator& GetMallocGpaForTesting() {
  return *gpa;
}

void InstallMallocHooks(size_t max_allocated_pages,
                        size_t num_metadata,
                        size_t total_pages,
                        size_t sampling_frequency,
                        GuardedPageAllocator::OutOfMemoryCallback callback) {
  static crash_reporter::CrashKeyString<24> malloc_crash_key(kMallocCrashKey);
  gpa = new GuardedPageAllocator();
  gpa->Init(max_allocated_pages, num_metadata, total_pages, std::move(callback),
            false);
  malloc_crash_key.Set(gpa->GetCrashKey());
  sampling_state.Init(sampling_frequency);
  base::allocator::InsertAllocatorDispatch(&g_allocator_dispatch);
}

}  // namespace internal

bool IsGwpAsanMallocAllocation(const void* ptr) {
  return internal::gpa && internal::gpa->PointerIsMine(ptr);
}

}  // namespace gwp_asan
