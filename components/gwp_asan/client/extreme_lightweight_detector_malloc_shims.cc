// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/extreme_lightweight_detector_malloc_shims.h"

#include <atomic>

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

extern AllocatorDispatch allocator_dispatch;

// By being implemented as a global with inline method definitions, method calls
// and member accesses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
//
// Note that this optimization has not been benchmarked. However since it is
// easy to do there is no reason to pay the extra cost.
SamplingState<EXTREMELIGHTWEIGHTDETECTOR> sampling_state;

using partition_alloc::internal::LightweightQuarantineBranch;
using partition_alloc::internal::LightweightQuarantineRoot;

ExtremeLightweightDetectorOptions init_options;

std::atomic<bool> is_quarantine_initialized = false;

// The PartitionRoot used by the PartitionAlloc-Everywhere (i.e. PartitionAlloc
// as malloc), which is also the target partition root of the quarantine.
// Since LightweightQuarantineRoot is designed to be used for a certain
// PartitionRoot and LightweightQuarantineBranch::Quarantine() cannot handle
// an object in an unknown root, the Extreme LUD performs only for the objects
// in this PartitionRoot.
partition_alloc::PartitionRoot* lightweight_quarantine_partition_root;
// A raw pointer to the LightweightQuarantineBranch as the fast path to the
// object. This bypasses the access check and indirect access due to the
// following std::optional and base::NoDestructor.
LightweightQuarantineBranch* lightweight_quarantine_branch;

// The memory storage for the quarantine root and branch to make them alive for
// the process lifetime. std::optional reserves the memory space without
// constructing the objects and allows to construct them lazily.
std::optional<LightweightQuarantineRoot> lightweight_quarantine_root_storage;
std::optional<base::NoDestructor<LightweightQuarantineBranch>>
    lightweight_quarantine_branch_storage;

// Sets up all we need and returns true, or returns false.
//
// We need to wait for the completion of `allocator_shim::ConfigurePartitions`
// so that the default PartitionRoot for `malloc` is fixed and the quarantine
// will be created for the default PartitionRoot. Until then, returns false.
bool TryInitSlow();

inline bool TryInit() {
  if (LIKELY(is_quarantine_initialized.load(std::memory_order_acquire))) {
    return true;
  }

  return TryInitSlow();
}

bool TryInitSlow() {
  if (!allocator_shim::internal::PartitionAllocMalloc::
          AllocatorConfigurationFinalized()) {
    // `allocator_shim::ConfigurePartitions` has not yet been called, and the
    // default PartitionRoot for `malloc` has not yet been fixed. Delay the
    // initialization of the quarantine.
    return false;
  }

  // Run the initialization process only once atomically (thread-safely).
  //
  // CAUTION: No deallocation is allowed here.
  //
  // This code runs only on the codepaths of deallocations (`free`, `delete`,
  // etc.) and _never_ runs on the codepaths of allocations (`malloc`, `new`,
  // etc.) because this allocator shim hooks only FreeFn, FreeDefiniteSizeFn,
  // etc. So, it's safe to allocate memory here as it doesn't recurse, however,
  // it's _NOT_ allowed to deallocate memory here as it _does_ recurse.
  //
  // The following code may allocate memory:
  // - `static` as a mutex may allocate memory.
  // - `LightweightQuarantineBranch` may allocate memory.
  //   `LightweightQuarantineBranch` has a data member of type `std::vector`,
  //   which may allocate.
  static bool init_once = [&]() -> bool {
    partition_alloc::PartitionRoot* partition_root =
        allocator_shim::internal::PartitionAllocMalloc::Allocator();

    lightweight_quarantine_partition_root = partition_root;
    lightweight_quarantine_root_storage.emplace(
        *partition_root, init_options.quarantine_capacity_in_bytes);
    lightweight_quarantine_branch_storage.emplace(
        lightweight_quarantine_root_storage->CreateBranch(
            /*lock_required=*/true));
    lightweight_quarantine_branch =
        lightweight_quarantine_branch_storage.value().get();

    is_quarantine_initialized.store(true, std::memory_order_release);

    return true;
  }();

  return init_once;
}

// Quarantines the object pointed to by `object`.
// Returns true when the object is quarantined (hence will be freed later) or
// freed immediately, otherwise false.
//
// CAUTION: No deallocation is allowed in this function because it causes
// a reentrancy issue.
inline bool Quarantine(void* object) {
  if (UNLIKELY(!TryInit())) {
    return false;
  }

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
  if (UNLIKELY(root != lightweight_quarantine_partition_root)) {
    // The LightweightQuarantineRoot is configured for
    // lightweight_quarantine_partition_root. We cannot quarantine an object
    // in other partition roots.
    return false;
  }

  size_t usable_size = root->GetSlotUsableSize(slot_span);
  ExtremeLightweightDetectorUtil::Zap(object, usable_size);

  uintptr_t slot_start = root->ObjectToSlotStart(object);
  lightweight_quarantine_branch->Quarantine(object, slot_span, slot_start);

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
  if (UNLIKELY(sampling_state.Sample())) {
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
  if (UNLIKELY(sampling_state.Sample())) {
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

void* AlignedMallocFn(const AllocatorDispatch* self,
                      size_t size,
                      size_t alignment,
                      void* context) {
  return self->next->aligned_malloc_function(self->next, size, alignment,
                                             context);
}

void* AlignedReallocFn(const AllocatorDispatch* self,
                       void* address,
                       size_t size,
                       size_t alignment,
                       void* context) {
  // Just the same as realloc, no support yet.
  return self->next->aligned_realloc_function(self->next, address, size,
                                              alignment, context);
}

void AlignedFreeFn(const AllocatorDispatch* self,
                   void* address,
                   void* context) {
  // As of 2024 Jan, only _aligned_free on Windows calls this function, so the
  // Extreme LUD doesn't support this for now.
  self->next->aligned_free_function(self->next, address, context);
}

AllocatorDispatch allocator_dispatch = {
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

void InstallExtremeLightweightDetectorHooks(
    const ExtremeLightweightDetectorOptions& options) {
  DCHECK(!init_options.sampling_frequency);
  DCHECK(options.sampling_frequency);

  init_options = options;

  sampling_state.Init(init_options.sampling_frequency);
  allocator_shim::InsertAllocatorDispatch(&allocator_dispatch);
}

partition_alloc::internal::LightweightQuarantineBranch&
GetEludQuarantineBranchForTesting() {
  CHECK(TryInit());

  return *lightweight_quarantine_branch;
}

}  // namespace gwp_asan::internal
