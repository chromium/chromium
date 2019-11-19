// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"

#include <inttypes.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"

namespace discardable_memory {
namespace {

// Default allocation size.
#if defined(OS_WIN) && defined(ARCH_CPU_32_BITS)
// On Windows 32 bit, use a smaller chunk, as address space fragmentation may
// make a 4MiB allocation impossible to fulfill in the browser process.
// See crbug.com/983348 for details.
const size_t kAllocationSize = 1 * 1024 * 1024;
#else
const size_t kAllocationSize = 4 * 1024 * 1024;
#endif

// Global atomic to generate unique discardable shared memory IDs.
base::AtomicSequenceNumber g_next_discardable_shared_memory_id;

class DiscardableMemoryImpl : public base::DiscardableMemory {
 public:
  DiscardableMemoryImpl(ClientDiscardableSharedMemoryManager* manager,
                        std::unique_ptr<DiscardableSharedMemoryHeap::Span> span)
      : manager_(manager), span_(std::move(span)), is_locked_(true) {}

  ~DiscardableMemoryImpl() override {
    if (is_locked_)
      manager_->UnlockSpan(span_.get());

    manager_->ReleaseSpan(std::move(span_));
  }

  // Overridden from base::DiscardableMemory:
  bool Lock() override {
    DCHECK(!is_locked_);

    if (!manager_->LockSpan(span_.get()))
      return false;

    is_locked_ = true;
    return true;
  }
  void Unlock() override {
    DCHECK(is_locked_);

    manager_->UnlockSpan(span_.get());
    is_locked_ = false;
  }
  void* data() const override {
    DCHECK(is_locked_);
    return reinterpret_cast<void*>(span_->start() * base::GetPageSize());
  }

  void DiscardForTesting() override {
    DCHECK(!is_locked_);
    span_->shared_memory()->Purge(base::Time::Now());
  }

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const override {
    return manager_->CreateMemoryAllocatorDump(span_.get(), name, pmd);
  }

 private:
  ClientDiscardableSharedMemoryManager* const manager_;
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryImpl);
};

void InitManagerMojoOnIO(
    mojo::Remote<mojom::DiscardableSharedMemoryManager>* manager_mojo,
    mojo::PendingRemote<mojom::DiscardableSharedMemoryManager> remote) {
  manager_mojo->Bind(std::move(remote));
}

void DeletedDiscardableSharedMemoryOnIO(
    mojo::Remote<mojom::DiscardableSharedMemoryManager>* manager_mojo,
    int32_t id) {
  (*manager_mojo)->DeletedDiscardableSharedMemory(id);
}

}  // namespace

ClientDiscardableSharedMemoryManager::ClientDiscardableSharedMemoryManager(
    mojo::PendingRemote<mojom::DiscardableSharedMemoryManager> manager,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)),
      manager_mojo_(std::make_unique<
                    mojo::Remote<mojom::DiscardableSharedMemoryManager>>()),
      heap_(new DiscardableSharedMemoryHeap(base::GetPageSize())) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ClientDiscardableSharedMemoryManager",
      base::ThreadTaskRunnerHandle::Get());
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InitManagerMojoOnIO, manager_mojo_.get(),
                                std::move(manager)));
}

ClientDiscardableSharedMemoryManager::~ClientDiscardableSharedMemoryManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  // TODO(reveman): Determine if this DCHECK can be enabled. crbug.com/430533
  // DCHECK_EQ(heap_->GetSize(), heap_->GetSizeOfFreeLists());
  if (heap_->GetSize())
    MemoryUsageChanged(0, 0);

  // Releasing the |heap_| before posting a task for deleting |manager_mojo_|.
  // It is because releasing |heap_| will invoke DeletedDiscardableSharedMemory
  // which needs |manager_mojo_|.
  heap_.reset();

  // Delete the |manager_mojo_| on IO thread, so any pending tasks on IO thread
  // will be executed before the |manager_mojo_| is deleted.
  bool posted = io_task_runner_->DeleteSoon(FROM_HERE, manager_mojo_.release());
  if (!posted)
    manager_mojo_.reset();
}

std::unique_ptr<base::DiscardableMemory>
ClientDiscardableSharedMemoryManager::AllocateLockedDiscardableMemory(
    size_t size) {
  base::AutoLock lock(lock_);

  DCHECK_NE(size, 0u);

  auto size_in_kb = static_cast<base::HistogramBase::Sample>(size / 1024);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.DiscardableAllocationSize",
                              size_in_kb,  // In KB
                              1,
                              4 * 1024 * 1024,  // 4 GB
                              50);

  // Round up to multiple of page size.
  size_t pages =
      std::max((size + base::GetPageSize() - 1) / base::GetPageSize(),
               static_cast<size_t>(1));

  // Default allocation size in pages.
  size_t allocation_pages = kAllocationSize / base::GetPageSize();

  size_t slack = 0;
  // When searching the free lists, allow a slack between required size and
  // free span size that is less or equal to kAllocationSize. This is to
  // avoid segments larger then kAllocationSize unless they are a perfect
  // fit. The result is that large allocations can be reused without reducing
  // the ability to discard memory.
  if (pages < allocation_pages)
    slack = allocation_pages - pages;

  size_t heap_size_prior_to_releasing_purged_memory = heap_->GetSize();
  for (;;) {
    // Search free lists for suitable span.
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> free_span =
        heap_->SearchFreeLists(pages, slack);
    if (!free_span)
      break;

    // Attempt to lock |free_span|. Delete span and search free lists again
    // if locking failed.
    if (free_span->shared_memory()->Lock(
            free_span->start() * base::GetPageSize() -
                reinterpret_cast<size_t>(free_span->shared_memory()->memory()),
            free_span->length() * base::GetPageSize()) ==
        base::DiscardableSharedMemory::FAILED) {
      DCHECK(!free_span->shared_memory()->IsMemoryResident());
      // We have to release purged memory before |free_span| can be destroyed.
      heap_->ReleasePurgedMemory();
      DCHECK(!free_span->shared_memory());
      continue;
    }

    free_span->set_is_locked(true);

    // Memory usage is guaranteed to have changed after having removed
    // at least one span from the free lists.
    MemoryUsageChanged(heap_->GetSize(), heap_->GetSizeOfFreeLists());

    return std::make_unique<DiscardableMemoryImpl>(this, std::move(free_span));
  }

  // Release purged memory to free up the address space before we attempt to
  // allocate more memory.
  heap_->ReleasePurgedMemory();

  // Make sure crash keys are up to date in case allocation fails.
  if (heap_->GetSize() != heap_size_prior_to_releasing_purged_memory)
    MemoryUsageChanged(heap_->GetSize(), heap_->GetSizeOfFreeLists());

  size_t pages_to_allocate =
      std::max(kAllocationSize / base::GetPageSize(), pages);
  size_t allocation_size_in_bytes = pages_to_allocate * base::GetPageSize();

  int32_t new_id = g_next_discardable_shared_memory_id.GetNext();

  // Ask parent process to allocate a new discardable shared memory segment.
  std::unique_ptr<base::DiscardableSharedMemory> shared_memory =
      AllocateLockedDiscardableSharedMemory(allocation_size_in_bytes, new_id);

  // Create span for allocated memory.
  // Spans are managed by |heap_| (the member of
  // the ClientDiscardableSharedMemoryManager), so it is safe to use
  // base::Unretained(this) here.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(heap_->Grow(
      std::move(shared_memory), allocation_size_in_bytes, new_id,
      base::Bind(
          &ClientDiscardableSharedMemoryManager::DeletedDiscardableSharedMemory,
          base::Unretained(this), new_id)));
  new_span->set_is_locked(true);

  // Unlock and insert any left over memory into free lists.
  if (pages < pages_to_allocate) {
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> leftover =
        heap_->Split(new_span.get(), pages);
    leftover->shared_memory()->Unlock(
        leftover->start() * base::GetPageSize() -
            reinterpret_cast<size_t>(leftover->shared_memory()->memory()),
        leftover->length() * base::GetPageSize());
    leftover->set_is_locked(false);
    heap_->MergeIntoFreeLists(std::move(leftover));
  }

  MemoryUsageChanged(heap_->GetSize(), heap_->GetSizeOfFreeLists());

  return std::make_unique<DiscardableMemoryImpl>(this, std::move(new_span));
}

bool ClientDiscardableSharedMemoryManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    base::trace_event::MemoryAllocatorDump* total_dump =
        pmd->CreateAllocatorDump(
            base::StringPrintf("discardable/child_0x%" PRIXPTR,
                               reinterpret_cast<uintptr_t>(this)));
    const size_t total_size = heap_->GetSize();
    const size_t freelist_size = heap_->GetSizeOfFreeLists();
    total_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          total_size - freelist_size);
    total_dump->AddScalar("freelist_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          freelist_size);
    return true;
  }

  return heap_->OnMemoryDump(pmd);
}

size_t ClientDiscardableSharedMemoryManager::GetBytesAllocated() const {
  base::AutoLock lock(lock_);
  return heap_->GetSize() - heap_->GetSizeOfFreeLists();
}

void ClientDiscardableSharedMemoryManager::ReleaseFreeMemory() {
  base::AutoLock lock(lock_);

  size_t heap_size_prior_to_releasing_memory = heap_->GetSize();

  // Release both purged and free memory.
  heap_->ReleasePurgedMemory();
  heap_->ReleaseFreeMemory();

  if (heap_->GetSize() != heap_size_prior_to_releasing_memory)
    MemoryUsageChanged(heap_->GetSize(), heap_->GetSizeOfFreeLists());
}

bool ClientDiscardableSharedMemoryManager::LockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  base::AutoLock lock(lock_);

  if (!span->shared_memory())
    return false;

  size_t offset = span->start() * base::GetPageSize() -
                  reinterpret_cast<size_t>(span->shared_memory()->memory());
  size_t length = span->length() * base::GetPageSize();

  switch (span->shared_memory()->Lock(offset, length)) {
    case base::DiscardableSharedMemory::SUCCESS:
      span->set_is_locked(true);
      return true;
    case base::DiscardableSharedMemory::PURGED:
      span->shared_memory()->Unlock(offset, length);
      span->set_is_locked(false);
      return false;
    case base::DiscardableSharedMemory::FAILED:
      return false;
  }

  NOTREACHED();
  return false;
}

void ClientDiscardableSharedMemoryManager::UnlockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  base::AutoLock lock(lock_);

  DCHECK(span->shared_memory());
  size_t offset = span->start() * base::GetPageSize() -
                  reinterpret_cast<size_t>(span->shared_memory()->memory());
  size_t length = span->length() * base::GetPageSize();

  span->set_is_locked(false);
  return span->shared_memory()->Unlock(offset, length);
}

void ClientDiscardableSharedMemoryManager::ReleaseSpan(
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> span) {
  base::AutoLock lock(lock_);

  // Delete span instead of merging it into free lists if memory is gone.
  if (!span->shared_memory())
    return;

  heap_->MergeIntoFreeLists(std::move(span));

  // Bytes of free memory changed.
  MemoryUsageChanged(heap_->GetSize(), heap_->GetSizeOfFreeLists());
}

base::trace_event::MemoryAllocatorDump*
ClientDiscardableSharedMemoryManager::CreateMemoryAllocatorDump(
    DiscardableSharedMemoryHeap::Span* span,
    const char* name,
    base::trace_event::ProcessMemoryDump* pmd) const {
  base::AutoLock lock(lock_);
  return heap_->CreateMemoryAllocatorDump(span, name, pmd);
}

std::unique_ptr<base::DiscardableSharedMemory>
ClientDiscardableSharedMemoryManager::AllocateLockedDiscardableSharedMemory(
    size_t size,
    int32_t id) {
  TRACE_EVENT2("renderer",
               "ClientDiscardableSharedMemoryManager::"
               "AllocateLockedDiscardableSharedMemory",
               "size", size, "id", id);
  base::UnsafeSharedMemoryRegion region;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::ScopedClosureRunner event_signal_runner(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ClientDiscardableSharedMemoryManager::AllocateOnIO,
                     base::Unretained(this), size, id, &region,
                     std::move(event_signal_runner)));
  // Waiting until IPC has finished on the IO thread.
  event.Wait();

  // This is likely address space exhaustion in the the browser process. We
  // don't want to crash the browser process for that, which is why the check is
  // here, and not there.
  //
  // TODO(crbug.com/983348): If this crashing a lot, fall back to a regular
  // allocation in the renderer process.
  if (!region.IsValid())
    base::TerminateBecauseOutOfMemory(size);

  auto memory =
      std::make_unique<base::DiscardableSharedMemory>(std::move(region));
  if (!memory->Map(size))
    base::TerminateBecauseOutOfMemory(size);
  return memory;
}

void ClientDiscardableSharedMemoryManager::AllocateOnIO(
    size_t size,
    int32_t id,
    base::UnsafeSharedMemoryRegion* region,
    base::ScopedClosureRunner closure_runner) {
  (*manager_mojo_)
      ->AllocateLockedDiscardableSharedMemory(
          static_cast<uint32_t>(size), id,
          base::BindOnce(
              &ClientDiscardableSharedMemoryManager::AllocateCompletedOnIO,
              base::Unretained(this), region, std::move(closure_runner)));
}

void ClientDiscardableSharedMemoryManager::AllocateCompletedOnIO(
    base::UnsafeSharedMemoryRegion* region,
    base::ScopedClosureRunner closure_runner,
    base::UnsafeSharedMemoryRegion ret_region) {
  *region = std::move(ret_region);
}

void ClientDiscardableSharedMemoryManager::DeletedDiscardableSharedMemory(
    int32_t id) {
  io_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&DeletedDiscardableSharedMemoryOnIO,
                                           manager_mojo_.get(), id));
}

void ClientDiscardableSharedMemoryManager::MemoryUsageChanged(
    size_t new_bytes_total,
    size_t new_bytes_free) const {
  static crash_reporter::CrashKeyString<24> discardable_memory_allocated(
      "discardable-memory-allocated");
  discardable_memory_allocated.Set(base::NumberToString(new_bytes_total));

  static crash_reporter::CrashKeyString<24> discardable_memory_free(
      "discardable-memory-free");
  discardable_memory_free.Set(base::NumberToString(new_bytes_free));
}

}  // namespace discardable_memory
