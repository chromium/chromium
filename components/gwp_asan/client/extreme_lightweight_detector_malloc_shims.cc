// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/extreme_lightweight_detector_malloc_shims.h"

#include "base/no_destructor.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "components/gwp_asan/common/extreme_lightweight_detector_util.h"
#include "partition_alloc/lightweight_quarantine.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"

namespace gwp_asan::internal {

namespace {

using allocator_shim::AllocatorDispatch;

extern AllocatorDispatch g_allocator_dispatch;

// By being implemented as a global with inline method definitions, method calls
// and member accesses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
//
// Note that this optimization has not been benchmarked. However since it is
// easy to do there is no reason to pay the extra cost.
SamplingState<EXTREMELIGHTWEIGHTDETECTOR> g_sampling_state;

using partition_alloc::internal::LightweightQuarantineBranch;
using partition_alloc::internal::LightweightQuarantineRoot;

inline partition_alloc::PartitionRoot* GetPartitionRoot() {
  return allocator_shim::internal::PartitionAllocMalloc::Allocator();
}

LightweightQuarantineRoot& EnsureQuarantineRoot() {
  constexpr size_t capacity_in_bytes = 256 * 1024;
  static LightweightQuarantineRoot s_root(*GetPartitionRoot(),
                                          capacity_in_bytes);
  return s_root;
}

LightweightQuarantineBranch& EnsureQuarantineBranch() {
  static base::NoDestructor<LightweightQuarantineBranch> s_branch(
      EnsureQuarantineRoot().CreateBranch(/*lock_required=*/true));
  return *s_branch;
}

// Quarantines the object pointed to by `object`.
// Returns true when the object is quarantined (hence will be freed later) or
// freed immediately, otherwise false.
inline bool Quarantine(void* object) {
  if (UNLIKELY(!object)) {
    return false;
  }

  if (UNLIKELY(!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(object)))) {
    return false;
  }

  // TODO(yukishiino): It may and may not be more performative to get the root
  // via `FromAddrInFirstSuperpage(internal::ObjectPtr2Addr(object))`.
  // See also:
  // https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/partition_root.h;l=1424-1434;drc=6b284da9be36f6edfdc0ddde4a031270c41096d8
  // Although in this case `slot_span` will be touched by `GetSlotUsableSize`.
  partition_alloc::internal::SlotSpanMetadata* slot_span =
      partition_alloc::internal::SlotSpanMetadata::FromObject(object);
  partition_alloc::PartitionRoot* root =
      partition_alloc::PartitionRoot::FromSlotSpanMetadata(slot_span);
  if (UNLIKELY(root != GetPartitionRoot())) {
    // The LightweightQuarantineRoot is configured for GetPartitionRoot().
    // We cannot quarantine an object in other partition roots.
    return false;
  }

  size_t usable_size = root->GetSlotUsableSize(slot_span);
  ExtremeLightweightDetectorUtil::Zap(object, usable_size);

  uintptr_t slot_start = root->ObjectToSlotStart(object);
  EnsureQuarantineBranch().Quarantine(object, slot_span, slot_start);
  return true;
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
  // realloc doesn't always deallocate memory, so the Extreme LUD doesn't
  // support realloc (for now).
  return self->next->realloc_function(self->next, address, size, context);
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  if (UNLIKELY(g_sampling_state.Sample())) {
    if (LIKELY(Quarantine(address))) {
      return;
    }
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
  // batch_free is rarely used, so the Extreme LUD doesn't support batch_free
  // (at least for now).
  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  if (UNLIKELY(g_sampling_state.Sample())) {
    if (LIKELY(Quarantine(address))) {
      return;
    }
  }
  self->next->free_definite_size_function(self->next, address, size, context);
}

void TryFreeDefaultFn(const AllocatorDispatch* self,
                      void* address,
                      void* context) {
  // try_free_default is rarely used, so the Extreme LUD doesn't support
  // try_free_default (at least for now).
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
  // Just the same as realloc, no support yet.
  return self->next->aligned_realloc_function(self->next, address, size,
                                              alignment, context);
}

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {
  // As of 2024 Jan, only _aligned_free on Windows calls this function, so the
  // Extreme LUD doesn't support this for now.
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
    nullptr,  // next
};

}  // namespace

void InstallExtremeLightweightDetectorHooks(size_t sampling_frequency) {
  DCHECK(allocator_shim::internal::PartitionAllocMalloc::
             AllocatorConfigurationFinalized());

  g_sampling_state.Init(sampling_frequency);
  allocator_shim::InsertAllocatorDispatch(&g_allocator_dispatch);
}

partition_alloc::internal::LightweightQuarantineBranch&
GetEludQuarantineBranchForTesting() {
  return EnsureQuarantineBranch();
}

}  // namespace gwp_asan::internal
