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

// `RandomEvictionQuarantine` is a quarantine mechanism for memory
// allocations. It works by replacing random allocations with new ones when
// adding to the quarantine.
class GWP_ASAN_EXPORT RandomEvictionQuarantineBase {
 public:
  RandomEvictionQuarantineBase(RandomEvictionQuarantineBase&) = delete;
  RandomEvictionQuarantineBase& operator=(const RandomEvictionQuarantineBase&) =
      delete;

  // Adds an allocation to the quarantine. Returns true if successful.
  bool Add(const AllocationInfo&);

 protected:
  RandomEvictionQuarantineBase(size_t max_allocation_count,
                               size_t max_total_size,
                               size_t total_size_high_water_mark,
                               size_t total_size_low_water_mark,
                               size_t eviction_chunk_size,
                               size_t eviction_task_interval_ms);
  ~RandomEvictionQuarantineBase();

 private:
  static constexpr size_t kMaxAllocationSize =
      32767;  // Maximum size for a single quarantined allocation.

  void PeriodicTrim();

  bool HasAllocationForTesting(void*) const;

  // Wrap external function calls into pure virtual functions for mocks.
  virtual void FinishFree(const AllocationInfo&) = 0;
  virtual void RecordAndZap(void* ptr, size_t size) = 0;

  const size_t max_allocation_count_;
  const size_t max_total_size_;
  const size_t total_size_high_water_mark_;
  const size_t total_size_low_water_mark_;
  const size_t eviction_chunk_size_;
  const base::TimeDelta eviction_task_interval_;

  mutable base::Lock lock_;
  std::atomic<size_t> total_size_ =
      0;  // Total size of allocations currently in quarantine.
          // Atomic for fast-path checks, but should be accessed
          // under the lock outside `PeriodicTrim`.
  std::vector<AllocationInfo> allocations_ GUARDED_BY(lock_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT})};
  base::RepeatingTimer timer_;

  friend class RandomEvictionQuarantineTest;
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, WrongSize);
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, SingleAllocation);
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, SlotReuse);
  FRIEND_TEST_ALL_PREFIXES(RandomEvictionQuarantineTest, Trim);
};

class GWP_ASAN_EXPORT RandomEvictionQuarantine final
    : public RandomEvictionQuarantineBase,
      public SharedState<RandomEvictionQuarantine> {
 public:
  RandomEvictionQuarantine(size_t max_allocation_count,
                           size_t max_total_size,
                           size_t total_size_high_water_mark,
                           size_t total_size_low_water_mark,
                           size_t eviction_chunk_size,
                           size_t eviction_task_interval_ms)
      : RandomEvictionQuarantineBase(max_allocation_count,
                                     max_total_size,
                                     total_size_high_water_mark,
                                     total_size_low_water_mark,
                                     eviction_chunk_size,
                                     eviction_task_interval_ms) {}

  // Since the allocator hooks cannot be uninstalled, and they access an
  // instance of this class, it's unsafe to ever destroy it outside unit tests.
  ~RandomEvictionQuarantine();

  void FinishFree(const AllocationInfo& info) override;
  void RecordAndZap(void* ptr, size_t size) override;

  friend class SharedState<RandomEvictionQuarantine>;
};

extern template class SharedState<RandomEvictionQuarantine>;

}  // namespace gwp_asan::internal::lud

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_RANDOM_EVICTION_QUARANTINE_H_
