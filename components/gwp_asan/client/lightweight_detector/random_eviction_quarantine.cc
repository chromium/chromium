// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"

#include "base/check_is_test.h"
#include "components/gwp_asan/client/thread_local_random_bit_generator.h"

namespace gwp_asan::internal::lud {

RandomEvictionQuarantineBase::RandomEvictionQuarantineBase(
    size_t max_allocation_count,
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
      allocations_(max_allocation_count) {
  DCHECK_GT(total_size_low_water_mark_, 0u);
  DCHECK_GT(total_size_high_water_mark_, total_size_low_water_mark_);
  DCHECK_GT(max_total_size_, total_size_high_water_mark_);
  DCHECK_GT(max_allocation_count_, 0u);
  DCHECK_GT(eviction_chunk_size_, 0u);

  // It's safe to pass `this` and `timer_` as `Unretained()` because this
  // class's instances aren't destructible in production code, as explained in
  // the `~RandomEvictionQuarantine()` comment.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          // Can't bind `Start` directly because it's overloaded.
          [](base::RepeatingTimer* timer, base::TimeDelta interval,
             base::RepeatingClosure closure) {
            timer->Start(FROM_HERE, interval, std::move(closure));
          },
          base::Unretained(&timer_), eviction_task_interval_,
          base::BindRepeating(&RandomEvictionQuarantineBase::PeriodicTrim,
                              base::Unretained(this))));
}

RandomEvictionQuarantineBase::~RandomEvictionQuarantineBase() = default;

bool RandomEvictionQuarantineBase::Add(const AllocationInfo& new_allocation) {
  if (new_allocation.size == 0 || new_allocation.size > kMaxAllocationSize)
      [[unlikely]] {
    return false;
  }

  // Record the deallocation event before quarantine to avoid racing with
  // trimming.
  RecordAndZap(new_allocation.address, new_allocation.size);

  // Pick an index to potentially replace before we acquire the lock.
  std::uniform_int_distribution<size_t> distribution(0,
                                                     max_allocation_count_ - 1);
  ThreadLocalRandomBitGenerator generator;
  size_t idx = distribution(generator);

  AllocationInfo evicted_allocation;
  bool update_succeeded = false;
  {
    base::AutoLock lock(lock_);

    AllocationInfo& entry = allocations_[idx];
    size_t tentative_total_size = total_size_.load(std::memory_order_relaxed) -
                                  entry.size + new_allocation.size;
    if (tentative_total_size <= max_total_size_) {
      total_size_.store(tentative_total_size, std::memory_order_relaxed);
      evicted_allocation = entry;
      entry = new_allocation;
      update_succeeded = true;
    }
  }

  if (evicted_allocation.address) {
    FinishFree(evicted_allocation);
  }

  return update_succeeded;
}

void RandomEvictionQuarantineBase::PeriodicTrim() {
  if (total_size_.load(std::memory_order_relaxed) <=
      total_size_high_water_mark_) {
    return;
  }

  std::vector<AllocationInfo> allocations_to_evict;
  allocations_to_evict.reserve(eviction_chunk_size_);

  std::uniform_int_distribution<size_t> distribution(0,
                                                     max_allocation_count_ - 1);
  ThreadLocalRandomBitGenerator generator;
  size_t evict_start_idx = distribution(generator);
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
      AllocationInfo& entry = allocations_[idx];

      if (!entry.address) {
        continue;
      }

      new_total_size -= entry.size;

      allocations_to_evict.push_back(entry);
      entry = AllocationInfo();
    }

    total_size_.store(new_total_size, std::memory_order_relaxed);
  }

  // TODO(glazunov): Since these allocations haven't been used for a while,
  // their memory is probably not in the CPU cache, so it might not be best to
  // keep them in the thread cache. Consider exposing the option to bypass the
  // thread cache in PartitionAlloc.
  for (auto allocation : allocations_to_evict) {
    FinishFree(allocation);
  }
}

bool RandomEvictionQuarantineBase::HasAllocationForTesting(
    void* requested_ptr) const {
  base::AutoLock lock(lock_);
  return std::any_of(
      allocations_.begin(), allocations_.end(),
      [&](const auto& entry) { return entry.address == requested_ptr; });
}

// Since the allocator hooks cannot be uninstalled, and they access an
// instance of this class, it's unsafe to ever destroy it outside unit tests.
RandomEvictionQuarantine::~RandomEvictionQuarantine() {
  CHECK_IS_TEST();
}

void RandomEvictionQuarantine::FinishFree(const AllocationInfo& info) {
  gwp_asan::internal::lud::FinishFree(info);
}

void RandomEvictionQuarantine::RecordAndZap(void* ptr, size_t size) {
  PoisonMetadataRecorder::Get()->RecordAndZap(ptr, size);
}

template class SharedState<RandomEvictionQuarantine>;

}  // namespace gwp_asan::internal::lud
