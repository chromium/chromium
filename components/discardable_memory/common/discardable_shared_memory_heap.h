// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISCARDABLE_MEMORY_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_
#define COMPONENTS_DISCARDABLE_MEMORY_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/containers/linked_list.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/discardable_memory/common/discardable_memory_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class DiscardableSharedMemory;
}

namespace discardable_memory {

DISCARDABLE_MEMORY_EXPORT extern const base::Feature
    kReleaseDiscardableFreeListPages;

// Implements a heap of discardable shared memory. An array of free lists
// is used to keep track of free blocks.
class DISCARDABLE_MEMORY_EXPORT DiscardableSharedMemoryHeap {
 private:
  class ScopedMemorySegment;

 public:
  class DISCARDABLE_MEMORY_EXPORT Span : public base::LinkNode<Span> {
   public:
    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;

    ~Span() = default;

    base::DiscardableSharedMemory* shared_memory() { return shared_memory_; }
    size_t start() const { return start_; }
    size_t length() const { return length_; }
    void set_is_locked(bool is_locked) { is_locked_ = is_locked; }

    // Marks all bytes in this Span as dirty, returns the number of pages
    // marked as dirty this way.
    size_t MarkAsDirty();
    // Marks all bytes in this Span as non-dirty, returning the number of
    // pages marked as non-dirty this way.
    size_t MarkAsClean();

    ScopedMemorySegment* GetScopedMemorySegmentForTesting() const;

   private:
    friend class DiscardableSharedMemoryHeap;

    Span(base::DiscardableSharedMemory* shared_memory,
         size_t start,
         size_t length,
         DiscardableSharedMemoryHeap::ScopedMemorySegment* memory_segment);

    const raw_ptr<DiscardableSharedMemoryHeap::ScopedMemorySegment,
                  DanglingUntriaged>
        memory_segment_;
    raw_ptr<base::DiscardableSharedMemory> shared_memory_;
    size_t start_;
    size_t length_;
    bool is_locked_;
  };

  DiscardableSharedMemoryHeap();

  DiscardableSharedMemoryHeap(const DiscardableSharedMemoryHeap&) = delete;
  DiscardableSharedMemoryHeap& operator=(const DiscardableSharedMemoryHeap&) =
      delete;

  ~DiscardableSharedMemoryHeap();

  // Grow heap using |shared_memory| and return a span for this new memory.
  // |shared_memory| must be aligned to the block size and |size| must be a
  // multiple of the block size. |deleted_callback| is called when
  // |shared_memory| has been deleted.
  std::unique_ptr<Span> Grow(
      std::unique_ptr<base::DiscardableSharedMemory> shared_memory,
      size_t size,
      int32_t id,
      base::OnceClosure deleted_callback);

  // Merge |span| into the free lists. This will coalesce |span| with
  // neighboring free spans when possible.
  void MergeIntoFreeLists(std::unique_ptr<Span> span);

  // Same as |MergeIntoFreeLists|, but doesn't mark the memory in the span as
  // dirtied (this is used for keeping track of how much memory is dirtied in
  // the freelist at any given time.
  void MergeIntoFreeListsClean(std::unique_ptr<Span> span);

  // Split an allocated span into two spans, one of length |blocks| followed
  // by another span of length "span->length - blocks" blocks. Modifies |span|
  // to point to the first span of length |blocks|. Return second span.
  std::unique_ptr<Span> Split(Span* span, size_t blocks);

  // Search free lists for span that satisfies the request for |blocks| of
  // memory. If found, the span is removed from the free list and returned.
  // |slack| determines the fitness requirement. Only spans that are less
  // or equal to |blocks| + |slack| are considered, worse fitting spans are
  // ignored.
  std::unique_ptr<Span> SearchFreeLists(size_t blocks, size_t slack);

  // Release free shared memory segments.
  void ReleaseFreeMemory();

  // Release shared memory segments that have been purged.
  void ReleasePurgedMemory();

  // Returns total bytes of memory in heap.
  size_t GetSize() const;

  // Returns bytes of memory currently in the free lists.
  size_t GetFreelistSize() const;

  // Dumps memory statistics for chrome://tracing.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd);

  // Returns a MemoryAllocatorDump for a given span on |pmd| with the size of
  // the span.
  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      Span* span,
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const;

  size_t dirty_freed_memory_page_count_ = 0;

 private:
  class DISCARDABLE_MEMORY_EXPORT ScopedMemorySegment {
   public:
    ScopedMemorySegment(
        DiscardableSharedMemoryHeap* heap,
        std::unique_ptr<base::DiscardableSharedMemory> shared_memory,
        size_t size,
        int32_t id,
        base::OnceClosure deleted_callback);

    ScopedMemorySegment(const ScopedMemorySegment&) = delete;
    ScopedMemorySegment& operator=(const ScopedMemorySegment&) = delete;

    ~ScopedMemorySegment();

    bool IsUsed() const;
    bool IsResident() const;

    bool ContainsSpan(Span* span) const;

    size_t CountMarkedPages() const;

    base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
        Span* span,
        size_t block_size,
        const char* name,
        base::trace_event::ProcessMemoryDump* pmd) const;

    // Used for dumping memory statistics from the segment to chrome://tracing.
    void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) const;

    size_t MarkPages(size_t start, size_t length, bool value);

   private:
    std::vector<bool> dirty_pages_;
    const raw_ptr<DiscardableSharedMemoryHeap> heap_;
    std::unique_ptr<base::DiscardableSharedMemory> shared_memory_;
    const size_t size_;
    const int32_t id_;
    base::OnceClosure deleted_callback_;
  };

  void InsertIntoFreeList(std::unique_ptr<Span> span);
  std::unique_ptr<Span> RemoveFromFreeList(Span* span);
  std::unique_ptr<Span> Carve(Span* span, size_t blocks);
  void RegisterSpan(Span* span);
  void UnregisterSpan(Span* span);
  bool IsMemoryUsed(const base::DiscardableSharedMemory* shared_memory,
                    size_t size);
  bool IsMemoryResident(const base::DiscardableSharedMemory* shared_memory);
  void ReleaseMemory(const base::DiscardableSharedMemory* shared_memory,
                     size_t size);

  absl::optional<size_t> GetResidentSize() const;

  // Dumps memory statistics about a memory segment for chrome://tracing.
  void OnMemoryDump(const base::DiscardableSharedMemory* shared_memory,
                    size_t size,
                    int32_t segment_id,
                    base::trace_event::ProcessMemoryDump* pmd);

  const size_t block_size_;
  size_t num_blocks_ = 0;
  size_t num_free_blocks_ = 0;

  // Vector of memory segments.
  std::vector<std::unique_ptr<ScopedMemorySegment>> memory_segments_;

  // Mapping from first/last block of span to Span instance.
  typedef std::unordered_map<size_t, Span*> SpanMap;
  SpanMap spans_;

  // Array of linked-lists with free discardable memory regions. For i < 256,
  // where the 1st entry is located at index 0 of the array, the kth entry
  // is a free list of runs that consist of k blocks. The 256th entry is a
  // free list of runs that have length >= 256 blocks.
  base::LinkedList<Span> free_spans_[256];
};

}  // namespace discardable_memory

#endif  // COMPONENTS_DISCARDABLE_MEMORY_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_
