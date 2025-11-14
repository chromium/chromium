// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/gwp_asan/client/extreme_lightweight_detector_quarantine.h"

#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/slot_start.h"

namespace gwp_asan::internal {

// Making this non-trivial dtor to allow use of `base::NoDestructor`.
// This explicit dtor is not needed in most build configurations because
// `raw_ptr<T>` has a non-trivial dtor. However, `raw_ptr<T>` does not guarantee
// it and we want to avoid the code here getting affected by `raw_ptr<T>`'s
// internal implementation.
// TODO(yukishiino): Make this trivially destructible.
// NOLINTNEXTLINE(modernize-use-equals-default)
ExtremeLightweightDetectorQuarantineRoot::
    ~ExtremeLightweightDetectorQuarantineRoot() {}

ExtremeLightweightDetectorQuarantineBranch
ExtremeLightweightDetectorQuarantineRoot::CreateBranch(
    const ExtremeLightweightDetectorQuarantineBranchConfig& config) {
  return ExtremeLightweightDetectorQuarantineBranch(*this, config);
}

ExtremeLightweightDetectorQuarantineBranch::
    ExtremeLightweightDetectorQuarantineBranch(
        Root& root,
        const ExtremeLightweightDetectorQuarantineBranchConfig& config)
    : root_(root),
      branch_capacity_in_bytes_(config.branch_capacity_in_bytes),
      leak_on_destruction_(config.leak_on_destruction) {
  to_be_freed_working_memory_ =
      partition_alloc::internal::ConstructAtInternalPartition<ToBeFreedArray>();
}

ExtremeLightweightDetectorQuarantineBranch::
    ExtremeLightweightDetectorQuarantineBranch(
        ExtremeLightweightDetectorQuarantineBranch&& b)
    : root_(b.root_),
      slots_(std::move(b.slots_)),
      branch_size_in_bytes_(b.branch_size_in_bytes_),
      branch_capacity_in_bytes_(
          b.branch_capacity_in_bytes_.load(std::memory_order_relaxed)),
      leak_on_destruction_(b.leak_on_destruction_) {
  b.branch_size_in_bytes_ = 0;
  to_be_freed_working_memory_.store(b.to_be_freed_working_memory_.exchange(
                                        nullptr, std::memory_order_relaxed),
                                    std::memory_order_relaxed);
}

ExtremeLightweightDetectorQuarantineBranch::
    ~ExtremeLightweightDetectorQuarantineBranch() {
  if (!leak_on_destruction_) {
    Purge();
  }
  if (ToBeFreedArray* to_be_freed = to_be_freed_working_memory_.exchange(
          nullptr, std::memory_order_relaxed)) {
    partition_alloc::internal::DestroyAtInternalPartition(to_be_freed);
  }
}

bool ExtremeLightweightDetectorQuarantineBranch::IsQuarantinedForTesting(
    void* object) {
  partition_alloc::internal::ScopedGuard guard(lock_);
  uintptr_t slot_start =
      partition_alloc::internal::SlotStart::Unchecked(object).Untag().value();
  for (const auto& slot : slots_) {
    if (slot.slot_start == slot_start) {
      return true;
    }
  }
  return false;
}

void ExtremeLightweightDetectorQuarantineBranch::SetCapacityInBytes(
    size_t capacity_in_bytes) {
  branch_capacity_in_bytes_.store(capacity_in_bytes, std::memory_order_relaxed);
}

void ExtremeLightweightDetectorQuarantineBranch::Purge() {
  partition_alloc::internal::ScopedGuard guard(lock_);
  PurgeInternal(0);
  slots_.shrink_to_fit();
}

bool ExtremeLightweightDetectorQuarantineBranch::Quarantine(
    void* object,
    partition_alloc::internal::SlotSpanMetadata* slot_span,
    uintptr_t slot_start,
    size_t usable_size) {
  DCHECK(usable_size == root_->allocator_root_->GetSlotUsableSize(slot_span));

  const size_t capacity_in_bytes =
      branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  if (capacity_in_bytes < usable_size) [[unlikely]] {
    // Even if this branch dequarantines all entries held by it, this entry
    // cannot fit within the capacity.
    root_->allocator_root_
        ->FreeNoHooksImmediate<partition_alloc::FreeFlags::kNone>(
            partition_alloc::internal::UntaggedSlotStart::Unchecked(slot_start)
                .Tag(),
            slot_span);
    root_->quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
    return false;
  }

  std::unique_ptr<
      ToBeFreedArray,
      partition_alloc::internal::InternalPartitionDeleter<ToBeFreedArray>>
      to_be_freed;
  size_t num_of_slots = 0;

  // Borrow the reserved working memory from to_be_freed_working_memory_,
  // and set nullptr to it indicating that it's in use.
  to_be_freed.reset(to_be_freed_working_memory_.exchange(nullptr));
  if (!to_be_freed) {
    // When the reserved working memory has already been in use by another
    // thread, fall back to allocate another chunk of working memory.
    to_be_freed.reset(partition_alloc::internal::ConstructAtInternalPartition<
                      ToBeFreedArray>());
  }

  {
    partition_alloc::internal::ScopedGuard guard(lock_);

    // Dequarantine some entries as required. Save the objects to be
    // deallocated into `to_be_freed`.
    PurgeInternalWithDefferedFree(capacity_in_bytes - usable_size, *to_be_freed,
                                  num_of_slots);

    // Put the entry onto the list.
    branch_size_in_bytes_ += usable_size;
    slots_.push_back({slot_start, usable_size});

    // Swap randomly so that the quarantine list remain shuffled.
    // This is not uniformly random, but sufficiently random.
    const size_t random_index = random_.RandUint32() % slots_.size();
    std::swap(slots_[random_index], slots_.back());
  }

  // Actually deallocate the dequarantined objects.
  BatchFree(*to_be_freed, num_of_slots);

  // Return the possibly-borrowed working memory to
  // to_be_freed_working_memory_. It doesn't matter much if it's really
  // borrowed or locally-allocated. The important facts are 1) to_be_freed is
  // non-null, and 2) to_be_freed_working_memory_ may likely be null (because
  // this or another thread has already borrowed it). It's simply good to make
  // to_be_freed_working_memory_ non-null whenever possible. Maybe yet another
  // thread would be about to borrow the working memory.
  to_be_freed.reset(
      to_be_freed_working_memory_.exchange(to_be_freed.release()));

  // Update stats (not locked).
  root_->count_.fetch_add(1, std::memory_order_relaxed);
  root_->size_in_bytes_.fetch_add(usable_size, std::memory_order_relaxed);
  root_->cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_->cumulative_size_in_bytes_.fetch_add(usable_size,
                                             std::memory_order_relaxed);
  return true;
}

ALWAYS_INLINE void ExtremeLightweightDetectorQuarantineBranch::PurgeInternal(
    size_t target_size_in_bytes) {
  int64_t freed_count = 0;
  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent
    // to picking random entry.
    const auto& to_free = slots_.back();
    size_t to_free_size = to_free.usable_size;

    const auto slot_start =
        partition_alloc::internal::UntaggedSlotStart::Checked(
            to_free.slot_start, &root_->allocator_root_.get());
    auto* slot_span =
        partition_alloc::internal::SlotSpanMetadata::FromSlotStart(
            slot_start, &root_->allocator_root_.get());
    DCHECK(slot_span ==
           partition_alloc::internal::SlotSpanMetadata::FromSlotStart(
               slot_start, &root_->allocator_root_.get()));

    DCHECK(to_free.slot_start);
    root_->allocator_root_
        ->FreeNoHooksImmediate<partition_alloc::FreeFlags::kNone>(
            slot_start.Tag(), slot_span);

    freed_count++;
    freed_size_in_bytes += to_free_size;
    branch_size_in_bytes_ -= to_free_size;

    slots_.pop_back();
  }

  root_->size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                  std::memory_order_relaxed);
  root_->count_.fetch_sub(freed_count, std::memory_order_relaxed);
}

ALWAYS_INLINE void
ExtremeLightweightDetectorQuarantineBranch::PurgeInternalWithDefferedFree(
    size_t target_size_in_bytes,
    ToBeFreedArray& to_be_freed,
    size_t& num_of_slots) {
  num_of_slots = 0;

  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent to
    // picking random entry.
    const QuarantineSlot& to_free = slots_.back();
    const size_t to_free_size = to_free.usable_size;

    to_be_freed[num_of_slots++] = to_free.slot_start;
    slots_.pop_back();

    freed_size_in_bytes += to_free_size;
    branch_size_in_bytes_ -= to_free_size;

    if (num_of_slots >= kMaxFreeTimesPerPurge) {
      break;
    }
  }

  root_->size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                  std::memory_order_relaxed);
  root_->count_.fetch_sub(num_of_slots, std::memory_order_relaxed);
}

ALWAYS_INLINE void ExtremeLightweightDetectorQuarantineBranch::BatchFree(
    const ToBeFreedArray& to_be_freed,
    size_t num_of_slots) {
  CHECK(num_of_slots <= kMaxFreeTimesPerPurge);
  for (size_t i = 0; i < num_of_slots; ++i) {
    const auto slot_start =
        partition_alloc::internal::UntaggedSlotStart::Checked(
            to_be_freed[i], &root_->allocator_root_.get());
    DCHECK(slot_start);
    auto* slot_span =
        partition_alloc::internal::SlotSpanMetadata::FromSlotStart(
            slot_start, &root_->allocator_root_.get());
    DCHECK(slot_span ==
           partition_alloc::internal::SlotSpanMetadata::FromSlotStart(
               slot_start, &root_->allocator_root_.get()));
    root_->allocator_root_
        ->FreeNoHooksImmediate<partition_alloc::FreeFlags::kNone>(
            slot_start.Tag(), slot_span);
  }
}

}  // namespace gwp_asan::internal
