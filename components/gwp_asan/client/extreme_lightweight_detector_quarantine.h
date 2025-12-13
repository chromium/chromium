// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extreme Lightweight Detector Quarantine provides a low-cost quarantine
// mechanism with following characteristics.
//
// - Built on PartitionAlloc: only supports allocations in a known root
// - As fast as PA: it just defers `Free()` handling and may benefit from thread
//   cache etc.
// - Thread-safe
// - No allocation time information: triggered on `Free()`
// - Don't use quarantined objects' payload - available for zapping
// - Don't allocate heap memory.
// - Flexible to support several applications
//
// `ExtremeLightweightDetectorQuarantineRoot` represents one quarantine system.
// `ExtremeLightweightDetectorQuarantineBranch` provides a quarantine request
// interface. It belongs to a `ExtremeLightweightDetectorQuarantineRoot` and
// there can be multiple instances (e.g. one branch for small objects and one
// branch for larger objects).
// ┌────────────────────────────┐
// │PartitionRoot               │
// └┬──────────────────────────┬┘
// ┌▽────────────────────────┐┌▽────────────────────┐
// │Quarantine Root 1        ││Quarantine Root 2    │
// └┬───────────┬───────────┬┘└──────────────┬──┬──┬┘
// ┌▽─────────┐┌▽─────────┐┌▽─────────┐      ▽  ▽  ▽
// │Branch 1  ││Branch 2  ││Branch 3  │
// └──────────┘└──────────┘└──────────┘

#ifndef COMPONENTS_GWP_ASAN_CLIENT_EXTREME_LIGHTWEIGHT_DETECTOR_QUARANTINE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_EXTREME_LIGHTWEIGHT_DETECTOR_QUARANTINE_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ref.h"
#include "base/rand_util.h"
#include "base/thread_annotations.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "components/gwp_asan/client/export.h"
#include "partition_alloc/internal_allocator_forward.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_stats.h"

namespace gwp_asan::internal {

struct ExtremeLightweightDetectorQuarantineBranchConfig {
  // Capacity for a branch in bytes.
  size_t branch_capacity_in_bytes = 0;
  // Leak quarantined allocations at exit.
  bool leak_on_destruction = false;
};

class ExtremeLightweightDetectorQuarantineBranch;

class GWP_ASAN_EXPORT ExtremeLightweightDetectorQuarantineRoot {
 public:
  explicit ExtremeLightweightDetectorQuarantineRoot(
      partition_alloc::PartitionRoot& allocator_root)
      : allocator_root_(allocator_root) {}
  ~ExtremeLightweightDetectorQuarantineRoot();

  ExtremeLightweightDetectorQuarantineBranch CreateBranch(
      const ExtremeLightweightDetectorQuarantineBranchConfig& config);

  partition_alloc::PartitionRoot& GetAllocatorRoot() {
    return allocator_root_.get();
  }

  void AccumulateStats(
      base::trace_event::MallocDumpProvider::ExtremeLUDStats& stats) const {
    stats.count += count_.load(std::memory_order_relaxed);
    stats.size_in_bytes += size_in_bytes_.load(std::memory_order_relaxed);
    stats.cumulative_count += cumulative_count_.load(std::memory_order_relaxed);
    stats.cumulative_size_in_bytes +=
        cumulative_size_in_bytes_.load(std::memory_order_relaxed);
    stats.quarantine_miss_count +=
        quarantine_miss_count_.load(std::memory_order_relaxed);
  }

 private:
  raw_ref<partition_alloc::PartitionRoot> allocator_root_;

  // Stats.
  std::atomic_size_t size_in_bytes_ = 0;
  std::atomic_size_t count_ = 0;  // Number of quarantined entries
  std::atomic_size_t cumulative_count_ = 0;
  std::atomic_size_t cumulative_size_in_bytes_ = 0;
  std::atomic_size_t quarantine_miss_count_ = 0;

  friend class ExtremeLightweightDetectorQuarantineBranch;
};

class GWP_ASAN_EXPORT ExtremeLightweightDetectorQuarantineBranch {
 public:
  using Root = ExtremeLightweightDetectorQuarantineRoot;

  ExtremeLightweightDetectorQuarantineBranch(
      Root& root,
      const ExtremeLightweightDetectorQuarantineBranchConfig& config);
  ExtremeLightweightDetectorQuarantineBranch(
      const ExtremeLightweightDetectorQuarantineBranch&) = delete;
  ExtremeLightweightDetectorQuarantineBranch& operator=(
      const ExtremeLightweightDetectorQuarantineBranch&) = delete;
  ExtremeLightweightDetectorQuarantineBranch(
      ExtremeLightweightDetectorQuarantineBranch&& b);
  ~ExtremeLightweightDetectorQuarantineBranch();

  // Quarantines an object. If the object is too large, this may return `false`,
  // meaning that quarantine request has failed (and freed immediately).
  // Otherwise, returns `true`.
  bool Quarantine(void* object,
                  partition_alloc::internal::SlotSpanMetadata* slot_span,
                  uintptr_t slot_start,
                  size_t usable_size);

  // Dequarantine all entries **held by this branch**.
  // It is possible that another branch with entries and it remains untouched.
  void Purge();

  // Determines this list contains an object.
  bool IsQuarantinedForTesting(void* object);

  Root& GetRoot() { return root_.get(); }

  size_t GetCapacityInBytes() {
    return branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  }
  // After shrinking the capacity, this branch may need to `Purge()` to meet the
  // requirement.
  void SetCapacityInBytes(size_t capacity_in_bytes);

 private:
  // `ToBeFreedArray` is used in `PurgeInternalInTwoPhases1of2` and
  // `PurgeInternalInTwoPhases2of2`. See the function comment about the purpose.
  // In order to avoid reentrancy issues, we must not deallocate any object in
  // `Quarantine`. So, std::vector is not an option. std::array doesn't
  // deallocate, plus, std::array has perf advantages.
  static constexpr size_t kMaxFreeTimesPerPurge = 1024;
  using ToBeFreedArray = std::array<uintptr_t, kMaxFreeTimesPerPurge>;

  // Try to dequarantine entries to satisfy below:
  //   root_.size_in_bytes_ <=  target_size_in_bytes
  // It is possible that this branch cannot satisfy the
  // request as it has control over only what it has. If you need to ensure the
  // constraint, call `Purge()` for each branch in sequence, synchronously.
  ALWAYS_INLINE void PurgeInternal(size_t target_size_in_bytes)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // In order to reduce thread contention, dequarantines entries in two phases:
  //   Phase 1) With the lock acquired, saves `slot_start`s of the quarantined
  //     objects in an array, and shrinks `slots_`. Then, releases the lock so
  //     that another thread can quarantine an object.
  //   Phase 2) Without the lock acquired, deallocates objects saved in the
  //     array in Phase 1. This may take some time, but doesn't block other
  //     threads.
  ALWAYS_INLINE void PurgeInternalWithDefferedFree(size_t target_size_in_bytes,
                                                   ToBeFreedArray& to_be_freed,
                                                   size_t& num_of_slots)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void BatchFree(const ToBeFreedArray& to_be_freed,
                               size_t num_of_slots);

  const raw_ref<Root> root_;

  partition_alloc::internal::Lock lock_;

  // Non-cryptographic random number generator.
  // Thread-unsafe so guarded by `lock_`.
  base::InsecureRandomGenerator random_ GUARDED_BY(lock_);

  // `slots_` hold quarantined entries.
  struct QuarantineSlot {
    uintptr_t slot_start;
    size_t usable_size;
  };
  std::vector<QuarantineSlot,
              partition_alloc::internal::InternalAllocator<QuarantineSlot>>
      slots_ GUARDED_BY(lock_);
  size_t branch_size_in_bytes_ GUARDED_BY(lock_) = 0;
  // Using `std::atomic` here so that other threads can update this value.
  std::atomic_size_t branch_capacity_in_bytes_;

  // This working memory is temporarily needed only while dequarantining
  // objects in slots_. However, allocating this working memory on stack may
  // cause stack overflow [1]. Plus, it's non- negligible perf penalty to
  // allocate and deallocate this working memory on heap only while
  // dequarantining. So, we reserve one chunk of working memory on heap during
  // the entire lifetime of this branch object and try to reuse this working
  // memory among threads. Only when thread contention occurs, we allocate and
  // deallocate another chunk of working memory. [1]
  // https://issues.chromium.org/issues/387508217
  std::atomic<ToBeFreedArray*> to_be_freed_working_memory_ = nullptr;

  const bool leak_on_destruction_ = false;
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_EXTREME_LIGHTWEIGHT_DETECTOR_QUARANTINE_H_
