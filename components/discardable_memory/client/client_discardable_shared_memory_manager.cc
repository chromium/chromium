// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"

namespace discardable_memory {
namespace {

// Global atomic to generate unique discardable shared memory IDs.
base::AtomicSequenceNumber g_next_discardable_shared_memory_id;

size_t GetDefaultAllocationSize() {
  const size_t kOneMegabyteInBytes = 1024 * 1024;

  // There is a trade-off between round-trip cost to the browser process and
  // memory usage overhead. 4MB is measured as the ideal size according to the
  // usage statistics. For low-end devices, we care about lowering the memory
  // usage and 1MB is good for the most basic cases.
  [[maybe_unused]] const size_t kDefaultAllocationSize =
      4 * kOneMegabyteInBytes;
  [[maybe_unused]] const size_t kDefaultLowEndDeviceAllocationSize =
      kOneMegabyteInBytes;

#if defined(ARCH_CPU_32_BITS) && !BUILDFLAG(IS_ANDROID)
  // On 32 bit architectures, use a smaller chunk, as address space
  // fragmentation may make a 4MiB allocation impossible to fulfill in the
  // browser process.  See crbug.com/983348 for details.
  //
  // Not on Android, since on this platform total number of file descriptors is
  // also a concern.
  return kDefaultLowEndDeviceAllocationSize;
#elif BUILDFLAG(IS_FUCHSIA)
  // Low end Fuchsia devices may be very constrained, so use smaller allocations
  // to save memory. See https://fxbug.dev/55760.
  return base::SysInfo::IsLowEndDevice() ? kDefaultLowEndDeviceAllocationSize
                                         : kDefaultAllocationSize;

#else
  return kDefaultAllocationSize;
#endif
}

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

constexpr base::TimeDelta
    ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge;
constexpr base::TimeDelta
    ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval;

ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::
    DiscardableMemoryImpl(
        ClientDiscardableSharedMemoryManager* manager,
        std::unique_ptr<DiscardableSharedMemoryHeap::Span> span)
    : manager_(manager), span_(std::move(span)) {
  DCHECK_NE(manager, nullptr);
}

ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::
    ~DiscardableMemoryImpl() {
  base::AutoLock lock(manager_->lock_);
  if (!span_) {
    DCHECK(!is_locked());
    return;
  }

  manager_->UnlockAndReleaseMemory(this, std::move(span_));
}

bool ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::Lock() {
  base::AutoLock lock(manager_->lock_);
  DCHECK(!is_locked());

  if (span_ && manager_->LockSpan(span_.get()))
    last_locked_ = base::TimeTicks();

  bool locked = is_locked();
  UMA_HISTOGRAM_BOOLEAN("Memory.Discardable.LockingSuccess", locked);

  return locked;
}

void ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::Unlock() {
  base::AutoLock lock(manager_->lock_);
  DCHECK(is_locked());
  DCHECK(span_);

  manager_->UnlockSpan(span_.get());
  last_locked_ = base::TimeTicks::Now();
}

std::unique_ptr<DiscardableSharedMemoryHeap::Span>
ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::Purge(
    base::TimeTicks min_ticks) {
  DCHECK(span_);

  if (is_locked())
    return nullptr;

  if (last_locked_ > min_ticks)
    return nullptr;

  return std::move(span_);
}

void* ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::data()
    const {
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(manager_->lock_);
    DCHECK(is_locked());
  }
#endif
  return span_->memory().data();
}

bool ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::is_locked()
    const {
  return last_locked_.is_null();
}

void ClientDiscardableSharedMemoryManager::DiscardableMemoryImpl::
    DiscardForTesting() {
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(manager_->lock_);
    DCHECK(!is_locked());
  }
#endif
  span_->shared_memory()->Purge(base::Time::Now());
}

base::trace_event::MemoryAllocatorDump* ClientDiscardableSharedMemoryManager::
    DiscardableMemoryImpl::CreateMemoryAllocatorDump(
        const char* name,
        base::trace_event::ProcessMemoryDump* pmd) const {
  base::AutoLock lock(manager_->lock_);
  return manager_->CreateMemoryAllocatorDump(span_.get(), name, pmd);
}

ClientDiscardableSharedMemoryManager::ClientDiscardableSharedMemoryManager(
    mojo::PendingRemote<mojom::DiscardableSharedMemoryManager> manager,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : ClientDiscardableSharedMemoryManager(io_task_runner) {
  manager_mojo_ =
      std::make_unique<mojo::Remote<mojom::DiscardableSharedMemoryManager>>();
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InitManagerMojoOnIO, manager_mojo_.get(),
                                std::move(manager)));
}

ClientDiscardableSharedMemoryManager::ClientDiscardableSharedMemoryManager(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : RefCountedDeleteOnSequence<ClientDiscardableSharedMemoryManager>(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      heap_(std::make_unique<DiscardableSharedMemoryHeap>()),
      io_task_runner_(std::move(io_task_runner)),
      manager_mojo_(nullptr) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ClientDiscardableSharedMemoryManager",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

ClientDiscardableSharedMemoryManager::~ClientDiscardableSharedMemoryManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  // Any memory allocated by a ClientDiscardableSharedMemoryManager must not be
  // touched after it is destroyed, or it will cause a use-after-free. This
  // check ensures that we stop before that can happen, instead of continuing
  // with dangling pointers.
  CHECK_EQ(heap_->GetSize(), heap_->GetFreelistSize());
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

void ClientDiscardableSharedMemoryManager::OnForegrounded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  foregrounded_ = true;
}

void ClientDiscardableSharedMemoryManager::OnBackgrounded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  foregrounded_ = false;
}

std::unique_ptr<base::DiscardableMemory>
ClientDiscardableSharedMemoryManager::AllocateLockedDiscardableMemory(
    size_t size) {
  base::AutoLock lock(lock_);

  if (!is_purge_scheduled_) {
    base::PostDelayedMemoryReductionTask(
        task_runner_, FROM_HERE,
        base::BindOnce(&ClientDiscardableSharedMemoryManager::ScheduledPurge,
                       this),
        kScheduledPurgeInterval);
    is_purge_scheduled_ = true;
  }

  DCHECK_NE(size, 0u);

  auto size_in_kb = static_cast<base::HistogramBase::Sample>(size / 1024);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.DiscardableAllocationSize",
                              size_in_kb,  // In KiB
                              1,
                              4 * 1024 * 1024,  // 4 GiB
                              50);

  // Round up to multiple of page size.
  size_t pages =
      std::max((size + base::GetPageSize() - 1) / base::GetPageSize(),
               static_cast<size_t>(1));

  static const size_t allocation_size = GetDefaultAllocationSize();
  DCHECK_EQ(allocation_size % base::GetPageSize(), 0u);
  // Default allocation size in pages.
  size_t allocation_pages = allocation_size / base::GetPageSize();

  size_t slack = 0;
  // When searching the free lists, allow a slack between required size and
  // free span size that is less or equal to |allocation_size|. This is to
  // avoid segments larger then |allocation_size| unless they are a perfect
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
            free_span->first_block() * base::GetPageSize(),
            free_span->num_blocks() * base::GetPageSize()) ==
        base::DiscardableSharedMemory::FAILED) {
      DCHECK(!free_span->shared_memory()->IsMemoryResident());
      // We have to release purged memory before |free_span| can be destroyed.
      heap_->ReleasePurgedMemory();
      DCHECK(!free_span->shared_memory());
      continue;
    }

    free_span->set_is_locked(true);

    if (pages >= allocation_pages) {
      UMA_HISTOGRAM_BOOLEAN("Memory.Discardable.LargeAllocationFromFreelist",
                            true);
    }

    // Memory usage is guaranteed to have changed after having removed
    // at least one span from the free lists.
    MemoryUsageChanged(heap_->GetSize(), heap_->GetFreelistSize());

    auto discardable_memory =
        std::make_unique<DiscardableMemoryImpl>(this, std::move(free_span));
    allocated_memory_.insert(discardable_memory.get());
    return std::move(discardable_memory);
  }

  // Release purged memory to free up the address space before we attempt to
  // allocate more memory.
  heap_->ReleasePurgedMemory();

  // Make sure crash keys are up to date in case allocation fails.
  if (heap_->GetSize() != heap_size_prior_to_releasing_purged_memory)
    MemoryUsageChanged(heap_->GetSize(), heap_->GetFreelistSize());

  size_t pages_to_allocate =
      std::max(allocation_size / base::GetPageSize(), pages);
  size_t allocation_size_in_bytes = pages_to_allocate * base::GetPageSize();

  int32_t new_id = g_next_discardable_shared_memory_id.GetNext();

  if (bytes_allocated_limit_for_testing_ &&
      heap_->GetSize() >= bytes_allocated_limit_for_testing_) {
    return nullptr;
  }

  // Ask parent process to allocate a new discardable shared memory segment.
  std::unique_ptr<base::DiscardableSharedMemory> shared_memory =
      AllocateLockedDiscardableSharedMemory(allocation_size_in_bytes, new_id);

  if (!shared_memory)
    return nullptr;

  // Create span for allocated memory.
  // Spans are managed by |heap_| (the member of
  // the ClientDiscardableSharedMemoryManager), so it is safe to use
  // base::Unretained(this) here.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(heap_->Grow(
      std::move(shared_memory), allocation_size_in_bytes, new_id,
      base::BindOnce(
          &ClientDiscardableSharedMemoryManager::DeletedDiscardableSharedMemory,
          base::Unretained(this), new_id)));
  new_span->set_is_locked(true);

  // Unlock and insert any left over memory into free lists.
  if (pages < pages_to_allocate) {
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> leftover =
        heap_->Split(new_span.get(), pages);
    leftover->shared_memory()->Unlock(
        leftover->first_block() * base::GetPageSize(),
        leftover->num_blocks() * base::GetPageSize());
    leftover->set_is_locked(false);
    heap_->MergeIntoFreeListsClean(std::move(leftover));
  }

  if (pages >= allocation_pages) {
    UMA_HISTOGRAM_BOOLEAN("Memory.Discardable.LargeAllocationFromFreelist",
                          false);
  }

  MemoryUsageChanged(heap_->GetSize(), heap_->GetFreelistSize());

  auto discardable_memory =
      std::make_unique<DiscardableMemoryImpl>(this, std::move(new_span));
  allocated_memory_.insert(discardable_memory.get());
  return std::move(discardable_memory);
}

bool ClientDiscardableSharedMemoryManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(lock_);
  if (foregrounded_) {
    const size_t total_size = heap_->GetSize() / 1024;                // in KiB
    const size_t freelist_size = heap_->GetFreelistSize() / 1024;     // in KiB

    base::UmaHistogramCounts1M("Memory.Discardable.FreelistSize.Foreground",
                               freelist_size);
    base::UmaHistogramCounts1M("Memory.Discardable.VirtualSize.Foreground",
                               total_size);
    base::UmaHistogramCounts1M("Memory.Discardable.Size.Foreground",
                               total_size - freelist_size);
  }

  return heap_->OnMemoryDump(args, pmd);
}

size_t ClientDiscardableSharedMemoryManager::GetBytesAllocated() const {
  base::AutoLock lock(lock_);
  return GetBytesAllocatedLocked();
}

size_t ClientDiscardableSharedMemoryManager::GetBytesAllocatedLocked() const {
  return heap_->GetSize() - heap_->GetFreelistSize();
}

void ClientDiscardableSharedMemoryManager::BackgroundPurge() {
  PurgeUnlockedMemory(base::TimeDelta());
}

void ClientDiscardableSharedMemoryManager::ScheduledPurge(
    base::MemoryReductionTaskContext task_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // From local testing and UMA, memory usually accumulates slowly in renderers,
  // and can sit idle for hours. We purge only the old memory, as this should
  // recover the memory without adverse latency effects. If |task_type| is
  // |kProactive|, we instead purge all memory.
  const base::TimeDelta min_age =
      task_type == base::MemoryReductionTaskContext::kProactive
          ? base::TimeDelta::Min()
          : ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge;
  PurgeUnlockedMemory(min_age);

  bool should_schedule = false;
  {
    base::AutoLock lock(lock_);
    should_schedule = GetBytesAllocatedLocked() != 0;
    is_purge_scheduled_ = should_schedule;
  }

  if (should_schedule) {
    base::PostDelayedMemoryReductionTask(
        task_runner_, FROM_HERE,
        base::BindOnce(&ClientDiscardableSharedMemoryManager::ScheduledPurge,
                       this),
        kScheduledPurgeInterval);
  }
}

void ClientDiscardableSharedMemoryManager::PurgeUnlockedMemory(
    base::TimeDelta min_age) {
  {
    base::AutoLock lock(lock_);

    auto now = base::TimeTicks::Now();

    // Iterate this way in order to avoid invalidating the iterator while
    // removing elements from |allocated_memory_| as we iterate over it.
    for (auto it = allocated_memory_.begin(); it != allocated_memory_.end();
         /* nop */) {
      auto prev = it++;
      DiscardableMemoryImpl* mem = *prev;

      // This assert is only required because the static checker can't figure
      // out that |mem->manager_->lock_| is the same as |this->lock_|, as
      // verified by the DCHECK.
      DCHECK_EQ(&lock_, &mem->manager_->lock_);
      mem->manager_->lock_.AssertAcquired();

      auto span = mem->Purge(now - min_age);
      if (span) {
        allocated_memory_.erase(prev);
        ReleaseSpan(std::move(span));
      }
    }
  }

  ReleaseFreeMemory();
}

void ClientDiscardableSharedMemoryManager::ReleaseFreeMemory() {
  TRACE_EVENT0("blink",
               "ClientDiscardableSharedMemoryManager::ReleaseFreeMemory()");
  base::AutoLock lock(lock_);
  size_t heap_size_prior_to_releasing_memory = heap_->GetSize();

  // Release both purged and free memory.
  heap_->ReleasePurgedMemory();
  heap_->ReleaseFreeMemory();

  if (heap_->GetSize() != heap_size_prior_to_releasing_memory)
    MemoryUsageChanged(heap_->GetSize(), heap_->GetFreelistSize());
}

bool ClientDiscardableSharedMemoryManager::LockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  if (!span->shared_memory())
    return false;

  size_t offset = span->first_block() * base::GetPageSize();
  size_t length = span->num_blocks() * base::GetPageSize();

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

  NOTREACHED_IN_MIGRATION();
  return false;
}

void ClientDiscardableSharedMemoryManager::UnlockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  DCHECK(span->shared_memory());
  size_t offset = span->first_block() * base::GetPageSize();
  size_t length = span->num_blocks() * base::GetPageSize();

  span->set_is_locked(false);
  return span->shared_memory()->Unlock(offset, length);
}

void ClientDiscardableSharedMemoryManager::UnlockAndReleaseMemory(
    DiscardableMemoryImpl* memory,
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> span) {
  memory->manager_->lock_.AssertAcquired();
  // lock_.AssertAcquired();
  if (memory->is_locked()) {
    UnlockSpan(span.get());
  }

  DCHECK(span);
  auto removed = allocated_memory_.erase(memory);
  DCHECK_EQ(removed, 1u);
  ReleaseSpan(std::move(span));
}

void ClientDiscardableSharedMemoryManager::ReleaseSpan(
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> span) {
  DCHECK(span);

  // Delete span instead of merging it into free lists if memory is gone.
  if (!span->shared_memory())
    return;

  heap_->MergeIntoFreeLists(std::move(span));

  // Bytes of free memory changed.
  MemoryUsageChanged(heap_->GetSize(), heap_->GetFreelistSize());
}

base::trace_event::MemoryAllocatorDump*
ClientDiscardableSharedMemoryManager::CreateMemoryAllocatorDump(
    DiscardableSharedMemoryHeap::Span* span,
    const char* name,
    base::trace_event::ProcessMemoryDump* pmd) const {
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
  static crash_reporter::CrashKeyString<24>
      discardable_memory_ipc_requested_size(
          "discardable-memory-ipc-requested-size");
  static crash_reporter::CrashKeyString<24> discardable_memory_ipc_error_cause(
      "discardable-memory-ipc-error-cause");

  base::UnsafeSharedMemoryRegion region;
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow;
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
  // don't want to crash the browser process for that, which is why the check
  // is here, and not there.
  if (!region.IsValid()) {
    discardable_memory_ipc_error_cause.Set("browser side");
    discardable_memory_ipc_requested_size.Set(base::NumberToString(size));
    return nullptr;
  }

  auto memory =
      std::make_unique<base::DiscardableSharedMemory>(std::move(region));
  if (!memory->Map(size)) {
    discardable_memory_ipc_error_cause.Set("client side");
    discardable_memory_ipc_requested_size.Set(base::NumberToString(size));
    return nullptr;
  }

  discardable_memory_ipc_error_cause.Clear();
  discardable_memory_ipc_requested_size.Clear();
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
