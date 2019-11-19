// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bits.h"
#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/common/pack_stack_trace.h"

#if defined(OS_ANDROID)
#include "components/crash/content/app/crashpad.h"  // nogncheck
#endif

#if defined(OS_MACOSX)
#include <pthread.h>
#endif

namespace gwp_asan {
namespace internal {

namespace {

// Report a tid that matches what crashpad collects which may differ from what
// base::PlatformThread::CurrentId() returns.
uint64_t ReportTid() {
#if !defined(OS_MACOSX)
  return base::PlatformThread::CurrentId();
#else
  uint64_t tid = base::kInvalidThreadId;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#endif
}

template <typename T>
T RandomEviction(std::vector<T>* list) {
  DCHECK(!list->empty());
  size_t rand = base::RandGenerator(list->size());
  T out = (*list)[rand];
  (*list)[rand] = list->back();
  list->pop_back();
  return out;
}

}  // namespace

// TODO: Delete out-of-line constexpr defininitons once C++17 is in use.
constexpr size_t GuardedPageAllocator::kOutOfMemoryCount;
constexpr size_t GuardedPageAllocator::kGpaAllocAlignment;

template <typename T>
void GuardedPageAllocator::SimpleFreeList<T>::Initialize(T max_entries) {
  max_entries_ = max_entries;
  free_list_.reserve(max_entries);
}

template <typename T>
bool GuardedPageAllocator::SimpleFreeList<T>::Allocate(T* out,
                                                       const char* type) {
  if (num_used_entries_ < max_entries_) {
    *out = num_used_entries_++;
    return true;
  }

  DCHECK_LE(free_list_.size(), max_entries_);
  *out = RandomEviction(&free_list_);
  return true;
}

template <typename T>
void GuardedPageAllocator::SimpleFreeList<T>::Free(T entry) {
  DCHECK_LT(free_list_.size(), max_entries_);
  free_list_.push_back(entry);
}

GuardedPageAllocator::PartitionAllocSlotFreeList::PartitionAllocSlotFreeList() =
    default;
GuardedPageAllocator::PartitionAllocSlotFreeList::
    ~PartitionAllocSlotFreeList() = default;

void GuardedPageAllocator::PartitionAllocSlotFreeList::Initialize(
    AllocatorState::SlotIdx max_entries) {
  max_entries_ = max_entries;
  type_mapping_.reserve(max_entries);
}

bool GuardedPageAllocator::PartitionAllocSlotFreeList::Allocate(
    AllocatorState::SlotIdx* out,
    const char* type) {
  if (num_used_entries_ < max_entries_) {
    type_mapping_.push_back(type);
    *out = num_used_entries_++;
    return true;
  }

  if (!free_list_.count(type) || free_list_[type].empty())
    return false;

  DCHECK_LE(free_list_[type].size(), max_entries_);
  *out = RandomEviction(&free_list_[type]);
  return true;
}

void GuardedPageAllocator::PartitionAllocSlotFreeList::Free(
    AllocatorState::SlotIdx entry) {
  DCHECK_LT(entry, num_used_entries_);
  free_list_[type_mapping_[entry]].push_back(entry);
}

GuardedPageAllocator::GuardedPageAllocator() {}

void GuardedPageAllocator::Init(size_t max_alloced_pages,
                                size_t num_metadata,
                                size_t total_pages,
                                OutOfMemoryCallback oom_callback,
                                bool is_partition_alloc) {
  CHECK_GT(max_alloced_pages, 0U);
  CHECK_LE(max_alloced_pages, num_metadata);
  CHECK_LE(num_metadata, AllocatorState::kMaxMetadata);
  CHECK_LE(num_metadata, total_pages);
  CHECK_LE(total_pages, AllocatorState::kMaxSlots);
  max_alloced_pages_ = max_alloced_pages;
  state_.num_metadata = num_metadata;
  state_.total_pages = total_pages;
  oom_callback_ = std::move(oom_callback);
  is_partition_alloc_ = is_partition_alloc;

  state_.page_size = base::GetPageSize();

  void* region = MapRegion();
  if (!region)
    PLOG(FATAL) << "Failed to reserve allocator region";

  state_.pages_base_addr = reinterpret_cast<uintptr_t>(region);
  state_.first_page_addr = state_.pages_base_addr + state_.page_size;
  state_.pages_end_addr = state_.pages_base_addr + RegionSize();

  {
    // Obtain this lock exclusively to satisfy the thread-safety annotations,
    // there should be no risk of a race here.
    base::AutoLock lock(lock_);
    free_metadata_.Initialize(num_metadata);
    if (is_partition_alloc_)
      free_slots_ = std::make_unique<PartitionAllocSlotFreeList>();
    else
      free_slots_ = std::make_unique<SimpleFreeList<AllocatorState::SlotIdx>>();
    free_slots_->Initialize(total_pages);
  }

  slot_to_metadata_idx_.resize(total_pages);
  for (size_t i = 0; i < total_pages; i++)
    slot_to_metadata_idx_[i] = AllocatorState::kInvalidMetadataIdx;
  state_.slot_to_metadata_addr =
      reinterpret_cast<uintptr_t>(&slot_to_metadata_idx_.front());

  metadata_ =
      std::make_unique<AllocatorState::SlotMetadata[]>(state_.num_metadata);
  state_.metadata_addr = reinterpret_cast<uintptr_t>(metadata_.get());

#if defined(OS_ANDROID)
  // Explicitly whitelist memory ranges the crash_handler needs to reads. This
  // is required for WebView because it has a stricter set of privacy
  // constraints on what it reads from the crashing process.
  for (auto& region : GetInternalMemoryRegions())
    crash_reporter::WhitelistMemoryRange(region.first, region.second);
#endif
}

std::vector<std::pair<void*, size_t>>
GuardedPageAllocator::GetInternalMemoryRegions() {
  std::vector<std::pair<void*, size_t>> regions;
  regions.push_back(std::make_pair(&state_, sizeof(state_)));
  regions.push_back(std::make_pair(
      metadata_.get(),
      sizeof(AllocatorState::SlotMetadata) * state_.num_metadata));
  regions.push_back(
      std::make_pair(slot_to_metadata_idx_.data(),
                     sizeof(AllocatorState::MetadataIdx) * state_.total_pages));
  return regions;
}

GuardedPageAllocator::~GuardedPageAllocator() {
  if (state_.total_pages)
    UnmapRegion();
}

void* GuardedPageAllocator::MapRegionHint() const {
#if defined(ARCH_CPU_64_BITS)
  // Mapping the GWP-ASan region in to the lower 32-bits of address space makes
  // it much more likely that a bad pointer dereference points into our region
  // and triggers a false positive report, so try to hint to the OS that we want
  // the region to be in the upper address space.
  static const uintptr_t kMinAddress = 1ULL << 32;
  static const uintptr_t kMaxAddress = 1ULL << 46;
  uint64_t rand = base::RandUint64() & (kMaxAddress - 1);
  if (rand < kMinAddress)
    rand += kMinAddress;
  return reinterpret_cast<void*>(rand & ~(state_.page_size - 1));
#else
  return nullptr;
#endif  // defined(ARCH_CPU_64_BITS)
}

void* GuardedPageAllocator::Allocate(size_t size,
                                     size_t align,
                                     const char* type) {
  if (!is_partition_alloc_)
    DCHECK_EQ(type, nullptr);

  if (!size || size > state_.page_size || align > state_.page_size)
    return nullptr;

  // Default alignment is size's next smallest power-of-two, up to
  // kGpaAllocAlignment.
  if (!align) {
    align =
        std::min(size_t{1} << base::bits::Log2Floor(size), kGpaAllocAlignment);
  }
  CHECK(base::bits::IsPowerOfTwo(align));

  AllocatorState::SlotIdx free_slot;
  AllocatorState::MetadataIdx free_metadata;
  if (!ReserveSlotAndMetadata(&free_slot, &free_metadata, type))
    return nullptr;

  uintptr_t free_page = state_.SlotToAddr(free_slot);
  MarkPageReadWrite(reinterpret_cast<void*>(free_page));

  size_t offset;
  if (free_slot & 1)
    // Return right-aligned allocation to detect overflows.
    offset = state_.page_size - base::bits::Align(size, align);
  else
    // Return left-aligned allocation to detect underflows.
    offset = 0;

  void* alloc = reinterpret_cast<void*>(free_page + offset);

  // Initialize slot metadata and only then update slot_to_metadata_idx so that
  // the mapping never points to an incorrect metadata mapping.
  RecordAllocationMetadata(free_metadata, size, alloc);
  {
    // Lock to avoid race with the slot_to_metadata_idx_ check/write in
    // ReserveSlotAndMetadata().
    base::AutoLock lock(lock_);
    slot_to_metadata_idx_[free_slot] = free_metadata;
  }

  return alloc;
}

void GuardedPageAllocator::Deallocate(void* ptr) {
  CHECK(PointerIsMine(ptr));

  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  AllocatorState::SlotIdx slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  AllocatorState::MetadataIdx metadata_idx = slot_to_metadata_idx_[slot];

  // Check for a call to free() with an incorrect pointer, e.g. the pointer does
  // not match the allocated pointer. This may occur with a bad free pointer or
  // an outdated double free when the metadata has expired.
  if (metadata_idx == AllocatorState::kInvalidMetadataIdx ||
      addr != metadata_[metadata_idx].alloc_ptr) {
    state_.free_invalid_address = addr;
    __builtin_trap();
  }

  // Check for double free.
  if (metadata_[metadata_idx].deallocation_occurred.exchange(true)) {
    state_.double_free_address = addr;
    // TODO(https://crbug.com/925447): The other thread may not be done writing
    // a stack trace so we could spin here until it's read; however, it's also
    // possible we are racing an allocation in the middle of
    // RecordAllocationMetadata. For now it's possible a racy double free could
    // lead to a bad stack trace, but no internal allocator corruption.
    __builtin_trap();
  }

  // Record deallocation stack trace/thread id before marking the page
  // inaccessible in case a use-after-free occurs immediately.
  RecordDeallocationMetadata(metadata_idx);
  MarkPageInaccessible(reinterpret_cast<void*>(state_.GetPageAddr(addr)));

  FreeSlotAndMetadata(slot, metadata_idx);
}

size_t GuardedPageAllocator::GetRequestedSize(const void* ptr) const {
  CHECK(PointerIsMine(ptr));
  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  AllocatorState::SlotIdx slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  AllocatorState::MetadataIdx metadata_idx = slot_to_metadata_idx_[slot];
#if !defined(OS_MACOSX)
  CHECK_LT(metadata_idx, state_.num_metadata);
  CHECK_EQ(addr, metadata_[metadata_idx].alloc_ptr);
#else
  // macOS core libraries call malloc_size() inside an allocation. The macOS
  // malloc_size() returns 0 when the pointer is not recognized.
  // https://crbug.com/946736
  if (metadata_idx == AllocatorState::kInvalidMetadataIdx ||
      addr != metadata_[metadata_idx].alloc_ptr)
    return 0;
#endif
  return metadata_[metadata_idx].alloc_size;
}

size_t GuardedPageAllocator::RegionSize() const {
  return (2 * state_.total_pages + 1) * state_.page_size;
}

bool GuardedPageAllocator::ReserveSlotAndMetadata(
    AllocatorState::SlotIdx* slot,
    AllocatorState::MetadataIdx* metadata_idx,
    const char* type) {
  base::AutoLock lock(lock_);
  if (num_alloced_pages_ == max_alloced_pages_ ||
      !free_slots_->Allocate(slot, type)) {
    if (!oom_hit_) {
      if (++consecutive_failed_allocations_ == kOutOfMemoryCount) {
        oom_hit_ = true;
        base::AutoUnlock unlock(lock_);
        std::move(oom_callback_).Run(total_allocations_ - kOutOfMemoryCount);
      }
    }
    return false;
  }

  CHECK(free_metadata_.Allocate(metadata_idx, nullptr));
  if (metadata_[*metadata_idx].alloc_ptr) {
    // Overwrite the outdated slot_to_metadata_idx mapping from the previous use
    // of this metadata if it's still valid.
    DCHECK(state_.PointerIsMine(metadata_[*metadata_idx].alloc_ptr));
    size_t old_slot = state_.GetNearestSlot(metadata_[*metadata_idx].alloc_ptr);
    if (slot_to_metadata_idx_[old_slot] == *metadata_idx)
      slot_to_metadata_idx_[old_slot] = AllocatorState::kInvalidMetadataIdx;
  }

  num_alloced_pages_++;
  total_allocations_++;
  consecutive_failed_allocations_ = 0;
  return true;
}

void GuardedPageAllocator::FreeSlotAndMetadata(
    AllocatorState::SlotIdx slot,
    AllocatorState::MetadataIdx metadata_idx) {
  DCHECK_LT(slot, state_.total_pages);
  DCHECK_LT(metadata_idx, state_.num_metadata);

  base::AutoLock lock(lock_);
  free_slots_->Free(slot);
  free_metadata_.Free(metadata_idx);

  DCHECK_GT(num_alloced_pages_, 0U);
  num_alloced_pages_--;
}

void GuardedPageAllocator::RecordAllocationMetadata(
    AllocatorState::MetadataIdx metadata_idx,
    size_t size,
    void* ptr) {
  metadata_[metadata_idx].alloc_size = size;
  metadata_[metadata_idx].alloc_ptr = reinterpret_cast<uintptr_t>(ptr);

  void* trace[AllocatorState::kMaxStackFrames];
  size_t len =
      base::debug::CollectStackTrace(trace, AllocatorState::kMaxStackFrames);
  metadata_[metadata_idx].alloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           metadata_[metadata_idx].stack_trace_pool,
           sizeof(metadata_[metadata_idx].stack_trace_pool) / 2);
  metadata_[metadata_idx].alloc.tid = ReportTid();
  metadata_[metadata_idx].alloc.trace_collected = true;

  metadata_[metadata_idx].dealloc.tid = base::kInvalidThreadId;
  metadata_[metadata_idx].dealloc.trace_len = 0;
  metadata_[metadata_idx].dealloc.trace_collected = false;
  metadata_[metadata_idx].deallocation_occurred = false;
}

void GuardedPageAllocator::RecordDeallocationMetadata(
    AllocatorState::MetadataIdx metadata_idx) {
  void* trace[AllocatorState::kMaxStackFrames];
  size_t len =
      base::debug::CollectStackTrace(trace, AllocatorState::kMaxStackFrames);
  metadata_[metadata_idx].dealloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           metadata_[metadata_idx].stack_trace_pool +
               metadata_[metadata_idx].alloc.trace_len,
           sizeof(metadata_[metadata_idx].stack_trace_pool) -
               metadata_[metadata_idx].alloc.trace_len);
  metadata_[metadata_idx].dealloc.tid = ReportTid();
  metadata_[metadata_idx].dealloc.trace_collected = true;
}

std::string GuardedPageAllocator::GetCrashKey() const {
  return base::StringPrintf("%zx", reinterpret_cast<uintptr_t>(&state_));
}

}  // namespace internal
}  // namespace gwp_asan
