// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/common/discardable_shared_memory_heap.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/page_size.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"

namespace discardable_memory {

BASE_FEATURE(kReleaseDiscardableFreeListPages,
             "ReleaseDiscardableFreeListPages",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

bool IsInFreeList(DiscardableSharedMemoryHeap::Span* span) {
  return span->previous() || span->next();
}

}  // namespace

DiscardableSharedMemoryHeap::Span::Span(
    base::DiscardableSharedMemory* shared_memory,
    size_t start,
    size_t length,
    DiscardableSharedMemoryHeap::ScopedMemorySegment* memory_segment)
    : memory_segment_(memory_segment),
      shared_memory_(shared_memory),
      start_(start),
      length_(length),
      is_locked_(false) {}

DiscardableSharedMemoryHeap::ScopedMemorySegment::ScopedMemorySegment(
    DiscardableSharedMemoryHeap* heap,
    std::unique_ptr<base::DiscardableSharedMemory> shared_memory,
    size_t size,
    int32_t id,
    base::OnceClosure deleted_callback)
    : dirty_pages_(std::vector<bool>(size / base::GetPageSize())),
      heap_(heap),
      shared_memory_(std::move(shared_memory)),
      size_(size),
      id_(id),
      deleted_callback_(std::move(deleted_callback)) {}

size_t DiscardableSharedMemoryHeap::Span::MarkAsClean() {
  return memory_segment_->MarkPages(start_, length_, false);
}

size_t DiscardableSharedMemoryHeap::Span::MarkAsDirty() {
  return memory_segment_->MarkPages(start_, length_, true);
}

DiscardableSharedMemoryHeap::ScopedMemorySegment*
DiscardableSharedMemoryHeap::Span::GetScopedMemorySegmentForTesting() const {
  return memory_segment_;
}

DiscardableSharedMemoryHeap::ScopedMemorySegment::~ScopedMemorySegment() {
  heap_->dirty_freed_memory_page_count_ -= MarkPages(
      reinterpret_cast<size_t>(shared_memory_->memory()) / base::GetPageSize(),
      dirty_pages_.size(), false);
  heap_->ReleaseMemory(shared_memory_.get(), size_);
  std::move(deleted_callback_).Run();
}

size_t DiscardableSharedMemoryHeap::ScopedMemorySegment::MarkPages(
    size_t start,
    size_t length,
    bool value) {
  if (!shared_memory_)
    return 0;

  const size_t offset =
      start -
      reinterpret_cast<size_t>(shared_memory_->memory()) / base::GetPageSize();

  size_t tmp = 0;
  for (size_t i = offset; i < offset + length; i++) {
    if (dirty_pages_[i] != value) {
      dirty_pages_[i] = value;
      tmp++;
    }
  }

  return tmp;
}

bool DiscardableSharedMemoryHeap::ScopedMemorySegment::IsUsed() const {
  return heap_->IsMemoryUsed(shared_memory_.get(), size_);
}

bool DiscardableSharedMemoryHeap::ScopedMemorySegment::IsResident() const {
  return heap_->IsMemoryResident(shared_memory_.get());
}

bool DiscardableSharedMemoryHeap::ScopedMemorySegment::ContainsSpan(
    Span* span) const {
  return shared_memory_.get() == span->shared_memory();
}

size_t DiscardableSharedMemoryHeap::ScopedMemorySegment::CountMarkedPages()
    const {
  return base::ranges::count(dirty_pages_, true);
}

base::trace_event::MemoryAllocatorDump*
DiscardableSharedMemoryHeap::ScopedMemorySegment::CreateMemoryAllocatorDump(
    Span* span,
    size_t block_size,
    const char* name,
    base::trace_event::ProcessMemoryDump* pmd) const {
  DCHECK_EQ(shared_memory_.get(), span->shared_memory());
  base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(span->length() * block_size));

  pmd->AddSuballocation(
      dump->guid(),
      base::StringPrintf("discardable/segment_%d/allocated_objects", id_));
  return dump;
}

void DiscardableSharedMemoryHeap::ScopedMemorySegment::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) const {
  heap_->OnMemoryDump(shared_memory_.get(), size_, id_, pmd);
}

DiscardableSharedMemoryHeap::DiscardableSharedMemoryHeap()
    : block_size_(base::GetPageSize()) {
  DCHECK_NE(block_size_, 0u);
  DCHECK(base::bits::IsPowerOfTwo(block_size_));
}

DiscardableSharedMemoryHeap::~DiscardableSharedMemoryHeap() {
  memory_segments_.clear();
  DCHECK_EQ(num_blocks_, 0u);
  DCHECK_EQ(num_free_blocks_, 0u);
  DCHECK(!base::Contains(free_spans_, false, &base::LinkedList<Span>::empty));
}

std::unique_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::Grow(
    std::unique_ptr<base::DiscardableSharedMemory> shared_memory,
    size_t size,
    int32_t id,
    base::OnceClosure deleted_callback) {
  // Memory must be aligned to block size.
  DCHECK(base::IsAligned(shared_memory->memory(), block_size_));
  DCHECK(base::IsAligned(size, block_size_));

  auto* raw_shared_memory = shared_memory.get();
  auto scoped_memory_segment = std::make_unique<ScopedMemorySegment>(
      this, std::move(shared_memory), size, id, std::move(deleted_callback));
  std::unique_ptr<Span> span(new Span(
      raw_shared_memory,
      reinterpret_cast<size_t>(raw_shared_memory->memory()) / block_size_,
      size / block_size_, scoped_memory_segment.get()));
  DCHECK(spans_.find(span->start_) == spans_.end());
  DCHECK(spans_.find(span->start_ + span->length_ - 1) == spans_.end());
  RegisterSpan(span.get());

  num_blocks_ += span->length_;

  // Start tracking if segment is resident by adding it to |memory_segments_|.
  memory_segments_.push_back(std::move(scoped_memory_segment));

  return span;
}

void DiscardableSharedMemoryHeap::MergeIntoFreeLists(
    std::unique_ptr<Span> span) {
  if (!base::FeatureList::IsEnabled(kReleaseDiscardableFreeListPages)) {
    dirty_freed_memory_page_count_ += span->MarkAsDirty();
  }
  MergeIntoFreeListsClean(std::move(span));
}

void DiscardableSharedMemoryHeap::MergeIntoFreeListsClean(
    std::unique_ptr<Span> span) {
  DCHECK(span->shared_memory_);

  if (base::FeatureList::IsEnabled(kReleaseDiscardableFreeListPages)) {
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Memory.Discardable.FreeListReleaseTime");
    // Release as much memory as possible before putting it into the freelists
    // in order to reduce their size. Getting this memory back is still much
    // cheaper than an IPC, while also saving us space in the freelists.
    //
    // The "+ 1" in the offset is for the SharedState that's at the start of
    // the DiscardableSharedMemory. See DiscardableSharedMemory for details on
    // what this is used for. We don't want to remove it, so we offset by an
    // extra page.
    size_t offset = (1 + span->start_) * base::GetPageSize() -
                    reinterpret_cast<size_t>(span->shared_memory()->memory());
    // Since we always offset by at least one page because of the SharedState,
    // our offset should never be 0.
    DCHECK_GT(offset, 0u);
    span->shared_memory()->ReleaseMemoryIfPossible(
        offset, span->length_ * base::GetPageSize());
  }

  // First add length of |span| to |num_free_blocks_|.
  num_free_blocks_ += span->length_;

  // Merge with previous span if possible.
  auto prev_it = spans_.find(span->start_ - 1);
  if (prev_it != spans_.end() && IsInFreeList(prev_it->second)) {
    std::unique_ptr<Span> prev = RemoveFromFreeList(prev_it->second);
    DCHECK_EQ(prev->start_ + prev->length_, span->start_);
    UnregisterSpan(prev.get());
    if (span->length_ > 1)
      spans_.erase(span->start_);
    span->start_ -= prev->length_;
    span->length_ += prev->length_;
    spans_[span->start_] = span.get();
  }

  // Merge with next span if possible.
  auto next_it = spans_.find(span->start_ + span->length_);
  if (next_it != spans_.end() && IsInFreeList(next_it->second)) {
    std::unique_ptr<Span> next = RemoveFromFreeList(next_it->second);
    DCHECK_EQ(next->start_, span->start_ + span->length_);
    UnregisterSpan(next.get());
    if (span->length_ > 1)
      spans_.erase(span->start_ + span->length_ - 1);
    span->length_ += next->length_;
    spans_[span->start_ + span->length_ - 1] = span.get();
  }

  InsertIntoFreeList(std::move(span));
}

std::unique_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::Split(Span* span, size_t blocks) {
  DCHECK(blocks);
  DCHECK_LT(blocks, span->length_);

  std::unique_ptr<Span> leftover(
      new Span(span->shared_memory_, span->start_ + blocks,
               span->length_ - blocks, span->memory_segment_));
  DCHECK(leftover->length_ == 1 ||
         spans_.find(leftover->start_) == spans_.end());
  RegisterSpan(leftover.get());
  spans_[span->start_ + blocks - 1] = span;
  span->length_ = blocks;
  return leftover;
}

std::unique_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::SearchFreeLists(size_t blocks, size_t slack) {
  DCHECK(blocks);

  size_t length = blocks;
  size_t max_length = blocks + slack;

  // Search array of free lists for a suitable span.
  while (length - 1 < std::size(free_spans_) - 1) {
    const base::LinkedList<Span>& free_spans = free_spans_[length - 1];
    if (!free_spans.empty()) {
      // Return the most recently used span located in tail.
      return Carve(free_spans.tail()->value(), blocks);
    }

    // Return early after surpassing |max_length|.
    if (++length > max_length)
      return nullptr;
  }

  const base::LinkedList<Span>& overflow_free_spans =
      free_spans_[std::size(free_spans_) - 1];

  // Search overflow free list for a suitable span. Starting with the most
  // recently used span located in tail and moving towards head.
  for (base::LinkNode<Span>* node = overflow_free_spans.tail();
       node != overflow_free_spans.end(); node = node->previous()) {
    Span* span = node->value();
    if (span->length_ >= blocks && span->length_ <= max_length)
      return Carve(span, blocks);
  }

  return nullptr;
}

void DiscardableSharedMemoryHeap::ReleaseFreeMemory() {
  // Erase all free segments after rearranging the segments in such a way
  // that used segments precede all free segments.
  memory_segments_.erase(
      std::partition(memory_segments_.begin(), memory_segments_.end(),
                     [](const std::unique_ptr<ScopedMemorySegment>& segment) {
                       return segment->IsUsed();
                     }),
      memory_segments_.end());
}

void DiscardableSharedMemoryHeap::ReleasePurgedMemory() {
  // Erase all purged segments after rearranging the segments in such a way
  // that resident segments precede all purged segments.
  memory_segments_.erase(
      std::partition(memory_segments_.begin(), memory_segments_.end(),
                     [](const std::unique_ptr<ScopedMemorySegment>& segment) {
                       return segment->IsResident();
                     }),
      memory_segments_.end());
}

size_t DiscardableSharedMemoryHeap::GetSize() const {
  return num_blocks_ * block_size_;
}

size_t DiscardableSharedMemoryHeap::GetFreelistSize() const {
  return num_free_blocks_ * block_size_;
}

absl::optional<size_t> DiscardableSharedMemoryHeap::GetResidentSize() const {
  size_t resident_size = 0;
  // Each member of |free_spans_| is a LinkedList of Spans. We need to iterate
  // over each of these.
  for (const base::LinkedList<Span>& span_list : free_spans_) {
    for (base::LinkNode<Span>* curr = span_list.head(); curr != span_list.end();
         curr = curr->next()) {
      Span* free_span = curr->value();
      // A given span over a piece of Shared Memory (which we will call
      // |shared_memory|) has Span::start_ initialized to a value equivalent
      // to reinterpret_cast<shared_memory->memory()) / block_size_.
      void* mem = reinterpret_cast<void*>(free_span->start() * block_size_);
      absl::optional<size_t> resident_in_span =
          base::trace_event::ProcessMemoryDump::CountResidentBytes(
              mem, free_span->length() * base::GetPageSize());
      if (!resident_in_span)
        return absl::nullopt;
      resident_size += resident_in_span.value();
    }
  }
  return resident_size;
}

bool DiscardableSharedMemoryHeap::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  // Keep track of some metrics that are specific to the
  // DiscardableSharedMemoryHeap, which aren't covered by the individual dumps
  // for each segment below.
  auto* total_dump = pmd->CreateAllocatorDump(base::StringPrintf(
      "discardable/child_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this)));
  const size_t freelist_size = GetFreelistSize();
  total_dump->AddScalar("freelist_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        freelist_size);
  total_dump->AddScalar("freelist_size_dirty",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        dirty_freed_memory_page_count_ * base::GetPageSize());
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    // These metrics (size and virtual size) are also reported by each
    // individual segment. If we report both, then the counts are artificially
    // inflated in detailed dumps, depending on aggregation (for instance, in
    // about:tracing's UI).
    const size_t total_size = GetSize();
    total_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          total_size - freelist_size);
    total_dump->AddScalar("virtual_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          total_size);
    auto resident_size = GetResidentSize();
    if (resident_size) {
      total_dump->AddScalar("resident_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            resident_size.value());
    }
  } else {
    // This iterates over all the memory allocated by the heap, and calls
    // |OnMemoryDump| for each. It does not contain any information about the
    // DiscardableSharedMemoryHeap itself.
    base::ranges::for_each(
        memory_segments_,
        [pmd](const std::unique_ptr<ScopedMemorySegment>& segment) {
          segment->OnMemoryDump(pmd);
        });
  }

  return true;
}

void DiscardableSharedMemoryHeap::InsertIntoFreeList(
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> span) {
  DCHECK(!IsInFreeList(span.get()));
  size_t index = std::min(span->length_, std::size(free_spans_)) - 1;

  free_spans_[index].Append(span.release());
}

std::unique_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::RemoveFromFreeList(Span* span) {
  DCHECK(IsInFreeList(span));
  span->RemoveFromList();
  return base::WrapUnique(span);
}

std::unique_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::Carve(Span* span, size_t blocks) {
  std::unique_ptr<Span> serving = RemoveFromFreeList(span);

  const size_t extra = serving->length_ - blocks;
  if (extra) {
    std::unique_ptr<Span> leftover(new Span(serving->shared_memory_,
                                            serving->start_ + blocks, extra,
                                            serving->memory_segment_));
    leftover->set_is_locked(false);
    DCHECK(extra == 1 || spans_.find(leftover->start_) == spans_.end());
    RegisterSpan(leftover.get());

    // No need to coalesce as the previous span of |leftover| was just split
    // and the next span of |leftover| was not previously coalesced with
    // |span|.
    InsertIntoFreeList(std::move(leftover));

    serving->length_ = blocks;
    spans_[serving->start_ + blocks - 1] = serving.get();
  }

  // |serving| is no longer in the free list, remove its length from
  // |num_free_blocks_|.
  DCHECK_GE(num_free_blocks_, serving->length_);
  num_free_blocks_ -= serving->length_;

  return serving;
}

void DiscardableSharedMemoryHeap::RegisterSpan(Span* span) {
  spans_[span->start_] = span;
  if (span->length_ > 1)
    spans_[span->start_ + span->length_ - 1] = span;
}

void DiscardableSharedMemoryHeap::UnregisterSpan(Span* span) {
  DCHECK(spans_.find(span->start_) != spans_.end());
  DCHECK_EQ(spans_[span->start_], span);
  spans_.erase(span->start_);
  if (span->length_ > 1) {
    DCHECK(spans_.find(span->start_ + span->length_ - 1) != spans_.end());
    DCHECK_EQ(spans_[span->start_ + span->length_ - 1], span);
    spans_.erase(span->start_ + span->length_ - 1);
  }
}

bool DiscardableSharedMemoryHeap::IsMemoryUsed(
    const base::DiscardableSharedMemory* shared_memory,
    size_t size) {
  size_t offset =
      reinterpret_cast<size_t>(shared_memory->memory()) / block_size_;
  size_t length = size / block_size_;
  DCHECK(spans_.find(offset) != spans_.end());
  Span* span = spans_[offset];
  DCHECK_LE(span->length_, length);
  // Memory is used if first span is not in free list or shorter than segment.
  return !IsInFreeList(span) || span->length_ != length;
}

bool DiscardableSharedMemoryHeap::IsMemoryResident(
    const base::DiscardableSharedMemory* shared_memory) {
  return shared_memory->IsMemoryResident();
}

void DiscardableSharedMemoryHeap::ReleaseMemory(
    const base::DiscardableSharedMemory* shared_memory,
    size_t size) {
  size_t offset =
      reinterpret_cast<size_t>(shared_memory->memory()) / block_size_;
  size_t end = offset + size / block_size_;
  while (offset < end) {
    DCHECK(spans_.find(offset) != spans_.end());
    Span* span = spans_[offset];
    DCHECK_EQ(span->shared_memory_, shared_memory);
    span->shared_memory_ = nullptr;
    UnregisterSpan(span);

    offset += span->length_;

    DCHECK_GE(num_blocks_, span->length_);
    num_blocks_ -= span->length_;

    // If |span| is in the free list, remove it and update |num_free_blocks_|.
    if (IsInFreeList(span)) {
      DCHECK_GE(num_free_blocks_, span->length_);
      num_free_blocks_ -= span->length_;
      RemoveFromFreeList(span);
    }
  }
}

void DiscardableSharedMemoryHeap::OnMemoryDump(
    const base::DiscardableSharedMemory* shared_memory,
    size_t size,
    int32_t segment_id,
    base::trace_event::ProcessMemoryDump* pmd) {
  size_t allocated_objects_count = 0;
  size_t allocated_objects_size_in_blocks = 0;
  size_t locked_objects_size_in_blocks = 0;
  size_t offset =
      reinterpret_cast<size_t>(shared_memory->memory()) / block_size_;
  size_t end = offset + size / block_size_;
  while (offset < end) {
    Span* span = spans_[offset];
    if (!IsInFreeList(span)) {
      allocated_objects_size_in_blocks += span->length_;
      locked_objects_size_in_blocks += span->is_locked_ ? span->length_ : 0;
      allocated_objects_count++;
    }
    offset += span->length_;
  }
  size_t allocated_objects_size_in_bytes =
      allocated_objects_size_in_blocks * block_size_;
  size_t locked_objects_size_in_bytes =
      locked_objects_size_in_blocks * block_size_;

  std::string segment_dump_name =
      base::StringPrintf("discardable/segment_%d", segment_id);
  base::trace_event::MemoryAllocatorDump* segment_dump =
      pmd->CreateAllocatorDump(segment_dump_name);
  segment_dump->AddScalar("virtual_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          size);

  base::trace_event::MemoryAllocatorDump* obj_dump =
      pmd->CreateAllocatorDump(segment_dump_name + "/allocated_objects");
  obj_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                      base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                      allocated_objects_count);
  obj_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      allocated_objects_size_in_bytes);
  obj_dump->AddScalar("locked_size",
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      locked_objects_size_in_bytes);

  // The memory is owned by the client process (current).
  shared_memory->CreateSharedMemoryOwnershipEdge(segment_dump, pmd,
                                                 /*is_owned=*/true);
}

base::trace_event::MemoryAllocatorDump*
DiscardableSharedMemoryHeap::CreateMemoryAllocatorDump(
    Span* span,
    const char* name,
    base::trace_event::ProcessMemoryDump* pmd) const {
  if (!span || !span->shared_memory()) {
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes, 0u);
    return dump;
  }

  auto it = base::ranges::find_if(
      memory_segments_,
      [span](const std::unique_ptr<ScopedMemorySegment>& segment) {
        return segment->ContainsSpan(span);
      });
  DCHECK(it != memory_segments_.end());
  return (*it)->CreateMemoryAllocatorDump(span, block_size_, name, pmd);
}

}  // namespace discardable_memory
