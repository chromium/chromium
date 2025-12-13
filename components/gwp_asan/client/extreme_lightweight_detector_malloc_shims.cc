// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/extreme_lightweight_detector_malloc_shims.h"

#include "partition_alloc/slot_start.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#include <atomic>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "components/gwp_asan/client/extreme_lightweight_detector_quarantine.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "components/gwp_asan/common/extreme_lightweight_detector_util.h"
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

ExtremeLightweightDetectorOptions init_options;

std::atomic<bool> is_quarantine_initialized = false;

// The PartitionRoot used by the PartitionAlloc-Everywhere (i.e. PartitionAlloc
// as malloc), which is also the target partition root of the quarantine.
// Since ExtremeLightweightDetectorQuarantineRoot is designed to be used for a
// certain PartitionRoot and
// ExtremeLightweightDetectorQuarantineBranch::QuarantineWithAcquiringLock()
// cannot handle an object in an unknown root, the Extreme LUD performs only for
// the objects in this PartitionRoot.
partition_alloc::PartitionRoot* lightweight_quarantine_partition_root;
// A raw pointer to the ExtremeLightweightDetectorQuarantineBranch as the fast
// path to the object. This bypasses the access check and indirect access due to
// the following std::optional and base::NoDestructor.
ExtremeLightweightDetectorQuarantineBranch*
    lightweight_quarantine_branch_for_small_objects;
ExtremeLightweightDetectorQuarantineBranch*
    lightweight_quarantine_branch_for_large_objects;

// The memory storage for the quarantine root and branch to make them alive for
// the process lifetime. std::optional reserves the memory space without
// constructing the objects and allows to construct them lazily.
std::optional<base::NoDestructor<ExtremeLightweightDetectorQuarantineRoot>>
    lightweight_quarantine_root_storage;
std::optional<base::NoDestructor<ExtremeLightweightDetectorQuarantineBranch>>
    lightweight_quarantine_branch_storage_for_small_objects;
std::optional<base::NoDestructor<ExtremeLightweightDetectorQuarantineBranch>>
    lightweight_quarantine_branch_storage_for_large_objects;

// Sets up all we need and returns true, or returns false.
//
// We need to wait for the completion of `allocator_shim::ConfigurePartitions`
// so that the default PartitionRoot for `malloc` is fixed and the quarantine
// will be created for the default PartitionRoot. Until then, returns false.
bool TryInitSlow();

inline bool TryInit() {
  if (is_quarantine_initialized.load(std::memory_order_acquire)) [[likely]] {
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
  // etc.) because this allocator shim hooks only FreeFn, FreeWithSizeFn,
  // etc. So, it's safe to allocate memory here as it doesn't recurse, however,
  // it's _NOT_ allowed to deallocate memory here as it _does_ recurse.
  //
  // The following code may allocate memory:
  // - `static` as a mutex may allocate memory.
  // - `ExtremeLightweightDetectorQuarantineBranch` may allocate memory.
  //   `ExtremeLightweightDetectorQuarantineBranch` has a data member of type
  //   `std::vector`, which may allocate.
  static bool init_once = [&]() -> bool {
    partition_alloc::PartitionRoot* partition_root =
        allocator_shim::internal::PartitionAllocMalloc::Allocator();
    lightweight_quarantine_partition_root = partition_root;
    lightweight_quarantine_root_storage.emplace(*partition_root);

    lightweight_quarantine_branch_storage_for_small_objects.emplace(
        lightweight_quarantine_root_storage->get()->CreateBranch(
            ExtremeLightweightDetectorQuarantineBranchConfig{
                .branch_capacity_in_bytes =
                    init_options.quarantine_capacity_for_small_objects_in_bytes,
            }));
    lightweight_quarantine_branch_for_small_objects =
        lightweight_quarantine_branch_storage_for_small_objects.value().get();

    lightweight_quarantine_branch_storage_for_large_objects.emplace(
        lightweight_quarantine_root_storage->get()->CreateBranch(
            ExtremeLightweightDetectorQuarantineBranchConfig{
                .branch_capacity_in_bytes =
                    init_options.quarantine_capacity_for_large_objects_in_bytes,
            }));
    lightweight_quarantine_branch_for_large_objects =
        lightweight_quarantine_branch_storage_for_large_objects.value().get();

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
  if (!TryInit()) [[unlikely]] {
    return false;
  }

  if (!object) [[unlikely]] {
    return false;
  }

  // This function is going to zap the memory region allocated for `object`,
  // but it can be cold in cache. So, prefetches it to avoid stall.
  PA_PREFETCH_FOR_WRITE(object);

  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(object))) [[unlikely]] {
    return false;
  }

  partition_alloc::internal::UntaggedSlotStart slot_start =
      partition_alloc::internal::SlotStart::Unchecked(object).Untag();

  // TODO(yukishiino): It may and may not be more performative to get the root
  // via `FromAddrInFirstSuperpage(internal::ObjectPtr2Addr(object))`.
  // See also:
  // https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/partition_root.h;l=1424-1434;drc=6b284da9be36f6edfdc0ddde4a031270c41096d8
  // Although in this case `slot_span` will be touched by `GetSlotUsableSize`.
  partition_alloc::internal::SlotSpanMetadata* slot_span =
      partition_alloc::internal::SlotSpanMetadata::FromSlotStart(slot_start);
  partition_alloc::PartitionRoot* root =
      partition_alloc::PartitionRoot::FromSlotSpanMetadata(slot_span);
  if (root != lightweight_quarantine_partition_root) [[unlikely]] {
    // The ExtremeLightweightDetectorQuarantineRoot is configured for
    // lightweight_quarantine_partition_root. We cannot quarantine an object
    // in other partition roots.
    return false;
  }

  if (lightweight_quarantine_partition_root->IsDirectMapped(slot_span))
      [[unlikely]] {
    // Direct-mapped allocations get immediately unmapped when being
    // deallocated, so the following accesses to the memory will cause crash
    // unless the address gets re-mapped again. Plus, direct-mapped allocations
    // tend to be very large, and zapping is more costful. So, we don't
    // quarantine the direct-mapped allocations.
    return false;
  }

  size_t usable_size = root->GetSlotUsableSize(slot_span);
  ExtremeLightweightDetectorUtil::Zap(object, usable_size);

  if (usable_size <= init_options.object_size_threshold_in_bytes) [[likely]] {
    lightweight_quarantine_branch_for_small_objects->Quarantine(
        object, slot_span, slot_start.value(), usable_size);
  } else {
    lightweight_quarantine_branch_for_large_objects->Quarantine(
        object, slot_span, slot_start.value(), usable_size);
  }

  return true;
}

void FreeFn(void* address, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (Quarantine(address)) [[likely]] {
      return;
    }
  }
  MUSTTAIL return allocator_dispatch.next->free_function(address, context);
}

void FreeWithSizeFn(void* address, size_t size, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (Quarantine(address)) [[likely]] {
      return;
    }
  }
  MUSTTAIL return allocator_dispatch.next->free_with_size_function(
      address, size, context);
}

void FreeWithAlignmentFn(void* address, size_t alignment, void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (Quarantine(address)) [[likely]] {
      return;
    }
  }
  MUSTTAIL return allocator_dispatch.next->free_with_alignment_function(
      address, alignment, context);
}

void FreeWithSizeAndAlignmentFn(void* address,
                                size_t size,
                                size_t alignment,
                                void* context) {
  if (sampling_state.Sample()) [[unlikely]] {
    if (Quarantine(address)) [[likely]] {
      return;
    }
  }
  MUSTTAIL return allocator_dispatch.next
      ->free_with_size_and_alignment_function(address, size, alignment,
                                              context);
}

AllocatorDispatch allocator_dispatch = {
    nullptr,  // alloc_function
    nullptr,  // alloc_unchecked_function
    nullptr,  // alloc_zero_initialized_function
    nullptr,  // alloc_zero_initialized_unchecked_function
    nullptr,  // alloc_aligned_function
    // realloc doesn't always deallocate memory, so the Extreme LUD doesn't
    // support realloc.
    nullptr,                     // realloc_function
    nullptr,                     // realloc_unchecked_function
    FreeFn,                      // free_function
    FreeWithSizeFn,              // free_with_size_function
    FreeWithAlignmentFn,         // free_with_alignment_function
    FreeWithSizeAndAlignmentFn,  // free_with_size_and_alignment_function
    nullptr,                     // get_size_estimate_function
    nullptr,                     // good_size_function
    nullptr,                     // claimed_address_function
    nullptr,                     // batch_malloc_function
    // batch_free is rarely used, so the Extreme LUD doesn't support batch_free
    // (at least for now).
    nullptr,  // batch_free_function
    // try_free_default is rarely used, so the Extreme LUD doesn't support
    // try_free_default (at least for now).
    nullptr,  // try_free_default_function
    nullptr,  // aligned_malloc_function
    nullptr,  // aligned_malloc_unchecked_function
    // The same reason with realloc_function.
    nullptr,  // aligned_realloc_function
    nullptr,  // aligned_realloc_unchecked_function
    // As of 2024 Jan, only _aligned_free on Windows calls this function. The
    // function is rarely used, so the Extreme LUD doesn't support this for now.
    nullptr,  // aligned_free_function
    nullptr   // next
};

[[maybe_unused]] base::trace_event::MallocDumpProvider::ExtremeLUDStatsSet
GetStats() {
  if (!lightweight_quarantine_branch_for_small_objects ||
      !lightweight_quarantine_branch_for_large_objects) {
    return {};  // Not yet initialized.
  }

  base::trace_event::MallocDumpProvider::ExtremeLUDStatsSet elud_stats_set;

  elud_stats_set.for_small_objects.capacity_in_bytes =
      lightweight_quarantine_branch_for_small_objects->GetCapacityInBytes();
  lightweight_quarantine_branch_for_small_objects->GetRoot().AccumulateStats(
      elud_stats_set.for_small_objects);

  elud_stats_set.for_large_objects.capacity_in_bytes =
      lightweight_quarantine_branch_for_large_objects->GetCapacityInBytes();
  lightweight_quarantine_branch_for_large_objects->GetRoot().AccumulateStats(
      elud_stats_set.for_large_objects);

  return elud_stats_set;
}

}  // namespace

void InstallExtremeLightweightDetectorHooks(
    const ExtremeLightweightDetectorOptions& options) {
  DCHECK(!init_options.sampling_frequency);
  DCHECK(options.sampling_frequency);

  init_options = options;

  sampling_state.Init(init_options.sampling_frequency);
  allocator_shim::InsertAllocatorDispatch(&allocator_dispatch);

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::trace_event::MallocDumpProvider::SetExtremeLUDGetStatsCallback(
      base::BindRepeating(GetStats));
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

ExtremeLightweightDetectorQuarantineBranch&
GetEludQuarantineBranchForSmallObjectsForTesting() {
  CHECK(TryInit());

  return *lightweight_quarantine_branch_for_small_objects;
}

ExtremeLightweightDetectorQuarantineBranch&
GetEludQuarantineBranchForLargeObjectsForTesting() {
  CHECK(TryInit());

  return *lightweight_quarantine_branch_for_large_objects;
}

}  // namespace gwp_asan::internal

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
