// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_
#define COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/common/allocator_state.h"

namespace gwp_asan {
namespace internal {

// This class encompasses the allocation and deallocation logic on top of the
// AllocatorState. Its members are not inspected or used by the crash handler.
//
// This class makes use of dynamically-sized arrays like std::vector<> to only
// allocate as much memory as we need; however, they only reserve memory at
// initialization-time so there is no risk of malloc reentrancy.
class GWP_ASAN_EXPORT GuardedPageAllocator {
 public:
  // Number of consecutive allocations that fail due to lack of available pages
  // before we call the OOM callback.
  static constexpr size_t kOutOfMemoryCount = 100;
  // Default maximum alignment for all returned allocations.
  static constexpr size_t kGpaAllocAlignment = 16;

  // Callback used to report the allocator running out of memory, reports the
  // number of successful allocations before running out of memory.
  using OutOfMemoryCallback = base::OnceCallback<void(size_t)>;

  // Does not allocate any memory for the allocator, to finish initializing call
  // Init().
  GuardedPageAllocator();

  // Configures this allocator to allocate up to max_alloced_pages pages at a
  // time, holding metadata for up to num_metadata allocations, from a pool of
  // total_pages pages, where:
  //   1 <= max_alloced_pages <= num_metadata <= kMaxMetadata
  //   num_metadata <= total_pages <= kMaxSlots
  //
  // The OOM callback is called the first time the allocator fails to allocate
  // kOutOfMemoryCount allocations consecutively due to lack of memory.
  void Init(size_t max_alloced_pages,
            size_t num_metadata,
            size_t total_pages,
            OutOfMemoryCallback oom_callback,
            bool is_partition_alloc);

  // On success, returns a pointer to size bytes of page-guarded memory. On
  // failure, returns nullptr. The allocation is not guaranteed to be
  // zero-filled. Failure can occur if memory could not be mapped or protected,
  // or if all guarded pages are already allocated.
  //
  // The align parameter specifies a power of two to align the allocation up to.
  // It must be less than or equal to the allocation size. If it's left as zero
  // it will default to the default alignment the allocator chooses.
  //
  // The type parameter should only be set for PartitionAlloc allocations.
  //
  // Preconditions: Init() must have been called.
  void* Allocate(size_t size, size_t align = 0, const char* type = nullptr);

  // Deallocates memory pointed to by ptr. ptr must have been previously
  // returned by a call to Allocate.
  void Deallocate(void* ptr);

  // Returns the size requested when ptr was allocated. ptr must have been
  // previously returned by a call to Allocate, and not have been deallocated.
  size_t GetRequestedSize(const void* ptr) const;

  // Retrieves the textual address of the shared allocator state required by the
  // crash handler.
  std::string GetCrashKey() const;

  // Returns internal memory used by the allocator (required for sanitization
  // on supported platforms.)
  std::vector<std::pair<void*, size_t>> GetInternalMemoryRegions();

  // Returns true if ptr points to memory managed by this class.
  inline bool PointerIsMine(const void* ptr) const {
    return state_.PointerIsMine(reinterpret_cast<uintptr_t>(ptr));
  }

 private:
  // Virtual base class representing a free list of entries T.
  template <typename T>
  class FreeList {
   public:
    FreeList() = default;
    virtual ~FreeList() = default;
    virtual void Initialize(T max_entries) = 0;
    virtual bool Allocate(T* out, const char* type) = 0;
    virtual void Free(T entry) = 0;
  };

  // Manages a free list of slot or metadata indices in the range
  // [0, max_entries). Access to SimpleFreeList objects must be synchronized.
  //
  // SimpleFreeList is specifically designed to pre-allocate data in Initialize
  // so that it never recurses into malloc/free during Allocate/Free.
  template <typename T>
  class SimpleFreeList : public FreeList<T> {
   public:
    ~SimpleFreeList() final = default;
    void Initialize(T max_entries) final;
    bool Allocate(T* out, const char* type) final;
    void Free(T entry) final;

   private:
    std::vector<T> free_list_;

    // Number of used entries. This counter ensures all free entries are used
    // before starting to use random eviction.
    T num_used_entries_ = 0;
    T max_entries_ = 0;
  };

  // Manages a free list of slot indices especially for PartitionAlloc.
  // Allocate() is type-aware so that once a page has been used to allocate
  // a given partition, it never reallocates an object of a different type on
  // that page. Access to this object must be synchronized.
  //
  // PartitionAllocSlotFreeList can perform malloc/free during Allocate/Free,
  // so it is not safe to use with malloc hooks!
  //
  // TODO(vtsyrklevich): Right now we allocate slots to partitions on a
  // first-come first-serve basis, this makes it likely that all slots will be
  // used up by common types first. Set aside a fixed amount of slots (~5%) for
  // one-off partitions so that we make sure to sample rare types as well.
  class PartitionAllocSlotFreeList : public FreeList<AllocatorState::SlotIdx> {
   public:
    PartitionAllocSlotFreeList();
    ~PartitionAllocSlotFreeList() final;
    void Initialize(AllocatorState::SlotIdx max_entries) final;
    bool Allocate(AllocatorState::SlotIdx* out, const char* type) final;
    void Free(AllocatorState::SlotIdx entry) final;

   private:
    std::vector<const char*> type_mapping_;
    std::map<const char*, std::vector<AllocatorState::SlotIdx>> free_list_;

    // Number of used entries. This counter ensures all free entries are used
    // before starting to use random eviction.
    AllocatorState::SlotIdx num_used_entries_ = 0;
    AllocatorState::SlotIdx max_entries_ = 0;
  };

  // Unmaps memory allocated by this class, if Init was called.
  ~GuardedPageAllocator();

  // Allocates/deallocates the virtual memory used for allocations.
  void* MapRegion();
  void UnmapRegion();

  // Provide a hint for MapRegion() on where to place the GWP-ASan region.
  void* MapRegionHint() const;

  // Returns the size of the virtual memory region used to store allocations.
  size_t RegionSize() const;

  // Mark page read-write.
  void MarkPageReadWrite(void*);

  // Mark page inaccessible and decommit the memory from use to save memory
  // used by the quarantine.
  void MarkPageInaccessible(void*);

  // On success, returns true and writes the reserved indices to |slot| and
  // |metadata_idx|. Otherwise returns false if no allocations are available.
  bool ReserveSlotAndMetadata(AllocatorState::SlotIdx* slot,
                              AllocatorState::MetadataIdx* metadata_idx,
                              const char* type) LOCKS_EXCLUDED(lock_);

  // Marks the specified slot and metadata as unreserved.
  void FreeSlotAndMetadata(AllocatorState::SlotIdx slot,
                           AllocatorState::MetadataIdx metadata_idx)
      LOCKS_EXCLUDED(lock_);

  // Record the metadata for an allocation or deallocation for a given metadata
  // index.
  ALWAYS_INLINE
  void RecordAllocationMetadata(AllocatorState::MetadataIdx metadata_idx,
                                size_t size,
                                void* ptr);
  ALWAYS_INLINE void RecordDeallocationMetadata(
      AllocatorState::MetadataIdx metadata_idx);

  // Allocator state shared with with the crash analyzer.
  AllocatorState state_;

  // Lock that synchronizes allocating/freeing slots between threads.
  base::Lock lock_;

  std::unique_ptr<FreeList<AllocatorState::SlotIdx>> free_slots_
      GUARDED_BY(lock_);
  SimpleFreeList<AllocatorState::MetadataIdx> free_metadata_ GUARDED_BY(lock_);

  // Number of currently-allocated pages.
  size_t num_alloced_pages_ GUARDED_BY(lock_) = 0;
  // Max number of concurrent allocations.
  size_t max_alloced_pages_ = 0;

  // Array of metadata (e.g. stack traces) for allocations.
  // TODO(vtsyrklevich): Use an std::vector<> here as well.
  std::unique_ptr<AllocatorState::SlotMetadata[]> metadata_;

  // Maps a slot index to a metadata index (or kInvalidMetadataIdx if no such
  // mapping exists.)
  std::vector<AllocatorState::MetadataIdx> slot_to_metadata_idx_;

  // Maintain a count of total allocations and consecutive failed allocations
  // to report allocator OOM.
  size_t total_allocations_ GUARDED_BY(lock_) = 0;
  size_t consecutive_failed_allocations_ GUARDED_BY(lock_) = 0;
  bool oom_hit_ GUARDED_BY(lock_) = false;
  OutOfMemoryCallback oom_callback_;

  bool is_partition_alloc_ = false;

  friend class BaseGpaTest;
  friend class CrashAnalyzerTest;
  FRIEND_TEST_ALL_PREFIXES(CrashAnalyzerTest, InternalError);
  FRIEND_TEST_ALL_PREFIXES(CrashAnalyzerTest, StackTraceCollection);

  DISALLOW_COPY_AND_ASSIGN(GuardedPageAllocator);
};

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_
