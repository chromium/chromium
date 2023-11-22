// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_RANDOM_EVICTION_QUARANTINE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_RANDOM_EVICTION_QUARANTINE_H_

#include <stddef.h>

#include <algorithm>
#include <atomic>
#include <new>
#include <vector>

#include "base/check_is_test.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/gwp_asan.h"
#include "components/gwp_asan/client/lightweight_detector/malloc_shims.h"
#include "components/gwp_asan/client/lightweight_detector/poison_metadata_recorder.h"
#include "components/gwp_asan/client/lightweight_detector/shared_state.h"

namespace gwp_asan::internal::lud {

// `RandomEvictionQuarantineImpl` is a quarantine mechanism for memory
// allocations. It works by replacing random allocations with new ones when
// adding to the quarantine.
//
// The class is templated to allow for mock testing
// without incurring a performance hit. To make it compile with mocks, we have
// to keep the function definitions in a header file.
template <typename MetadataRecorder, typename ShimSupport>
class GWP_ASAN_EXPORT RandomEvictionQuarantineImpl
    : public SharedState<
          RandomEvictionQuarantineImpl<MetadataRecorder, ShimSupport>> {
 public:
  RandomEvictionQuarantineImpl(RandomEvictionQuarantineImpl&) = delete;
  RandomEvictionQuarantineImpl& operator=(const RandomEvictionQuarantineImpl&) =
      delete;

  // Adds an allocation to the quarantine. Returns true if successful.
  bool Add(void* ptr);

 private:
  static constexpr size_t kMaxAllocationSize =
      32767;  // Maximum size for a single quarantined allocation.

  // Since the allocator hooks cannot be uninstalled, and they access an
  // instance of this class, it's unsafe to ever destroy it outside unit tests.
  RandomEvictionQuarantineImpl(size_t max_allocation_count,
                               size_t max_total_size,
                               size_t total_size_high_water_mark,
                               size_t total_size_low_water_mark,
                               size_t eviction_chunk_size,
                               size_t eviction_task_interval_ms);
  ~RandomEvictionQuarantineImpl();

  void PeriodicTrim();

  bool HasAllocationForTesting(void*);

  const size_t max_allocation_count_;
  const size_t max_total_size_;
  const size_t total_size_high_water_mark_;
  const size_t total_size_low_water_mark_;
  const size_t eviction_chunk_size_;
  const base::TimeDelta eviction_task_interval_;

  base::Lock lock_;
  std::atomic<size_t> total_size_ =
      0;  // Total size of allocations currently in quarantine.
          // Atomic for fast-path checks, but should be accessed
          // under the lock outside `PeriodicTrim`.
  std::vector<void*> allocation_ptrs_ GUARDED_BY(lock_);
  std::vector<uint16_t> allocation_sizes_
      GUARDED_BY(lock_);  // Storing sizes separately for memory efficiency.
  static_assert(kMaxAllocationSize <= std::numeric_limits<uint16_t>::max(),
                "Allocation sizes must fit into uint16_t");

  scoped_refptr<base::SequencedTaskRunner> task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT})};
  base::RepeatingTimer timer_;

  friend class SharedState<
      RandomEvictionQuarantineImpl<MetadataRecorder, ShimSupport>>;
  friend class RandomEvictionQuarantineTest;
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, WrongSize);
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, SingleAllocation);
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, SlotReuse);
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, Trim);
};

template <typename MetadataRecorder, typename ShimSupport>
RandomEvictionQuarantineImpl<MetadataRecorder, ShimSupport>::
    RandomEvictionQuarantineImpl(size_t max_allocation_count,
                                 size_t max_total_size,
                                 size_t total_size_high_water_mark,
                                 size_t total_size_low_water_mark,
                                 size_t eviction_chunk_size,
                                 size_t eviction_task_interval_ms)
    : max_allocation_count_(max_allocation_count),
      max_total_size_(max_total_size),
      total_size_high_water_mark_(total_size_high_water_mark),
      total_size_low_water_mark_(total_size_low_water_mark),
      eviction_chunk_size_(eviction_chunk_size),
      eviction_task_interval_(base::Milliseconds(eviction_task_interval_ms)),
      allocation_ptrs_(max_allocation_count),
      allocation_sizes_(max_allocation_count) {
  DCHECK_GT(total_size_low_water_mark_, 0u);
  DCHECK_GT(total_size_high_water_mark_, total_size_low_water_mark_);
  DCHECK_GT(max_total_size_, total_size_high_water_mark_);
  DCHECK_GT(max_allocation_count_, 0u);
  DCHECK_GT(eviction_chunk_size_, 0u);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          // Can't bind `Start` directly because it's overloaded.
          [](base::RepeatingTimer* timer, base::TimeDelta interval,
             base::RepeatingClosure closure) {
            timer->Start(FROM_HERE, interval, std::move(closure));
          },
          base::Unretained(&timer_), eviction_task_interval_,
          base::BindRepeating(&RandomEvictionQuarantineImpl::PeriodicTrim,
                              base::Unretained(this))));
}

template <typename MetadataRecorder, typename ShimSupport>
RandomEvictionQuarantineImpl<MetadataRecorder,
                             ShimSupport>::~RandomEvictionQuarantineImpl() {
  // Since the allocator hooks cannot be uninstalled, and they access an
  // instance of this class, it's unsafe to ever destroy it outside unit tests.
  CHECK_IS_TEST();
}

template <typename MetadataRecorder, typename ShimSupport>
bool RandomEvictionQuarantineImpl<MetadataRecorder, ShimSupport>::Add(
    void* ptr) {
  size_t size = ShimSupport::NextGetSizeEstimate(ptr);

  if (UNLIKELY(size == 0 || size > kMaxAllocationSize)) {
    return false;
  }

  // Record the deallocation event before quarantine to avoid racing with
  // trimming.
  MetadataRecorder::Get()->RecordAndZap(ptr, size);

  // Pick an index to potentially replace before we acquire the lock.
  size_t idx = base::RandGenerator(max_allocation_count_);

  void* evicted_ptr = nullptr;
  bool update_succeeded = false;
  {
    base::AutoLock lock(lock_);

    size_t evicted_size = allocation_sizes_[idx];
    size_t tentative_total_size =
        total_size_.load(std::memory_order_relaxed) - evicted_size + size;
    if (tentative_total_size <= max_total_size_) {
      total_size_.store(tentative_total_size, std::memory_order_relaxed);
      evicted_ptr = allocation_ptrs_[idx];

      allocation_sizes_[idx] = size;
      allocation_ptrs_[idx] = ptr;

      update_succeeded = true;
    }
  }

  if (evicted_ptr) {
    ShimSupport::NextFree(evicted_ptr);
  }

  return update_succeeded;
}

template <typename MetadataRecorder, typename ShimSupport>
void RandomEvictionQuarantineImpl<MetadataRecorder,
                                  ShimSupport>::PeriodicTrim() {
  if (total_size_.load(std::memory_order_relaxed) <=
      total_size_high_water_mark_) {
    return;
  }

  std::vector<void*> ptrs_to_evict;
  ptrs_to_evict.reserve(eviction_chunk_size_);

  size_t evict_start_idx = base::RandGenerator(max_allocation_count_);
  {
    base::AutoLock lock(lock_);

    // Trim even if the `total_size_` became smaller than the high watermark
    // while we were acquiring the lock. Otherwise, we'll have to trim soon
    // anyway.
    size_t new_total_size = total_size_.load(std::memory_order_relaxed);

    for (size_t i = 0; i < eviction_chunk_size_; ++i) {
      if (new_total_size <= total_size_low_water_mark_) {
        break;
      }

      size_t idx = (evict_start_idx + i) % max_allocation_count_;

      if (!allocation_ptrs_[idx]) {
        continue;
      }

      new_total_size -= allocation_sizes_[idx];
      ptrs_to_evict.push_back(allocation_ptrs_[idx]);

      allocation_sizes_[idx] = 0;
      allocation_ptrs_[idx] = nullptr;
    }

    total_size_.store(new_total_size, std::memory_order_relaxed);
  }

  for (auto* ptr : ptrs_to_evict) {
    ShimSupport::NextFree(ptr);
  }
}

template <typename MetadataRecorder, typename ShimSupport>
bool RandomEvictionQuarantineImpl<MetadataRecorder, ShimSupport>::
    HasAllocationForTesting(void* requested_ptr) {
  base::AutoLock lock(lock_);
  return std::find(allocation_ptrs_.begin(), allocation_ptrs_.end(),
                   requested_ptr) != allocation_ptrs_.end();
}

// Default implementation.
using RandomEvictionQuarantine =
    RandomEvictionQuarantineImpl<PoisonMetadataRecorder, MallocShimSupport>;

extern template class SharedState<RandomEvictionQuarantine>;

}  // namespace gwp_asan::internal::lud

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_RANDOM_EVICTION_QUARANTINE_H_
