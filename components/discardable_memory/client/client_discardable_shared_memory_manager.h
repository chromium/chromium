// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
#define COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/functional/callback_helpers.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/discardable_memory/common/discardable_memory_export.h"
#include "components/discardable_memory/common/discardable_shared_memory_heap.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace discardable_memory {

// Implementation of DiscardableMemoryAllocator that allocates
// discardable memory segments through the browser process.
class DISCARDABLE_MEMORY_EXPORT ClientDiscardableSharedMemoryManager
    : public base::DiscardableMemoryAllocator,
      public base::trace_event::MemoryDumpProvider,
      public base::RefCountedDeleteOnSequence<
          ClientDiscardableSharedMemoryManager> {
 public:
  ClientDiscardableSharedMemoryManager(
      mojo::PendingRemote<mojom::DiscardableSharedMemoryManager> manager,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ClientDiscardableSharedMemoryManager(
      const ClientDiscardableSharedMemoryManager&) = delete;
  ClientDiscardableSharedMemoryManager& operator=(
      const ClientDiscardableSharedMemoryManager&) = delete;

  // Overridden from base::DiscardableMemoryAllocator:
  std::unique_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override LOCKS_EXCLUDED(lock_);

  // Overridden from base::DiscardableMemoryAllocator:
  size_t GetBytesAllocated() const override LOCKS_EXCLUDED(lock_);

  // Overridden from base::DiscardableMemoryAllocator:
  // Release memory and associated resources that have been purged.
  void ReleaseFreeMemory() override LOCKS_EXCLUDED(lock_);

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override
      LOCKS_EXCLUDED(lock_);

  // Purge all unlocked memory that was allocated by this manager.
  void BackgroundPurge();

  // Change the state of this to either backgrounded or foregrounded. These
  // states should match the state that is found in |RenderThreadImpl|. We
  // initially set the state to backgrounded, since we may not know the state we
  // are in when we construct this. This avoids accidentally collecting data
  // from this while we are in the background, at the cost of potentially losing
  // some data near the time this is created.
  void OnForegrounded();
  void OnBackgrounded();

  void SetBytesAllocatedLimitForTesting(size_t limit) {
    bytes_allocated_limit_for_testing_ = limit;
  }

  // Anything younger than |kMinAgeForScheduledPurge| is not discarded when we
  // do our periodic purge.
  static constexpr base::TimeDelta kMinAgeForScheduledPurge = base::Minutes(5);

  // The expected cost of purging should be very small (< 1ms), so it can be
  // scheduled frequently. However, we don't purge memory that has been touched
  // recently (see: |BackgroundPurge()| and |kMinAgeForScheduledPurge|), so
  // there is no benefit to scheduling this more than once per minute.
  static constexpr base::TimeDelta kScheduledPurgeInterval = base::Minutes(1);

  // These fields are only protected for testing, they would otherwise be
  // private. Everything else should be either public or private.
 protected:
  friend class base::RefCountedDeleteOnSequence<
      ClientDiscardableSharedMemoryManager>;
  friend class base::DeleteHelper<ClientDiscardableSharedMemoryManager>;

  ~ClientDiscardableSharedMemoryManager() override;
  explicit ClientDiscardableSharedMemoryManager(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  mutable base::Lock lock_;
  std::unique_ptr<DiscardableSharedMemoryHeap> heap_ GUARDED_BY(lock_);
  bool is_purge_scheduled_ GUARDED_BY(lock_) = false;

 private:
  friend class TestClientDiscardableSharedMemoryManager;
  class DiscardableMemoryImpl : public base::DiscardableMemory {
   public:
    DiscardableMemoryImpl(
        ClientDiscardableSharedMemoryManager* manager,
        std::unique_ptr<DiscardableSharedMemoryHeap::Span> span);
    ~DiscardableMemoryImpl() override;

    DiscardableMemoryImpl(const DiscardableMemoryImpl&) = delete;
    DiscardableMemoryImpl& operator=(const DiscardableMemoryImpl&) = delete;

    // Overridden from base::DiscardableMemory:
    bool Lock() override LOCKS_EXCLUDED(manager_->lock_);
    void Unlock() override LOCKS_EXCLUDED(manager_->lock_);
    void* data() const override LOCKS_EXCLUDED(manager_->lock_);
    void DiscardForTesting() override LOCKS_EXCLUDED(manager_->lock_);
    base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
        const char* name,
        base::trace_event::ProcessMemoryDump* pmd) const override;

    // Returns |span_| if it has been unlocked since at least |min_ticks|,
    // otherwise nullptr.
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> Purge(
        base::TimeTicks min_ticks) EXCLUSIVE_LOCKS_REQUIRED(manager_->lock_);

   private:
    bool is_locked() const EXCLUSIVE_LOCKS_REQUIRED(manager_->lock_);

    friend class ClientDiscardableSharedMemoryManager;
    // We need to ensure that |manager_| outlives |this|, to avoid a
    // use-after-free.
    scoped_refptr<ClientDiscardableSharedMemoryManager> const manager_;
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> span_;
    // Set to an invalid base::TimeTicks when |this| is Lock()-ed, and to
    // |TimeTicks::Now()| each time |this| is Unlock()-ed.
    base::TimeTicks last_locked_ GUARDED_BY(manager_->lock_);
  };

  struct Statistics {
    size_t total_size;
    size_t freelist_size;
  };

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      DiscardableSharedMemoryHeap::Span* span,
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Purge any unlocked memory from foreground that hasn't been touched in a
  // while.
  void ScheduledPurge(base::MemoryReductionTaskContext task_type)
      LOCKS_EXCLUDED(lock_);

  // This is only virtual for testing.
  virtual std::unique_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size, int32_t id);
  void AllocateOnIO(size_t size,
                    int32_t id,
                    base::UnsafeSharedMemoryRegion* region,
                    base::ScopedClosureRunner closure_runner);
  void AllocateCompletedOnIO(base::UnsafeSharedMemoryRegion* region,
                             base::ScopedClosureRunner closure_runner,
                             base::UnsafeSharedMemoryRegion ret_region);

  // This is only virtual for testing.
  virtual void DeletedDiscardableSharedMemory(int32_t id);
  void MemoryUsageChanged(size_t new_bytes_allocated,
                          size_t new_bytes_free) const;

  // Releases all unlocked memory that was last locked at least |min_age| ago.
  void PurgeUnlockedMemory(base::TimeDelta min_age) LOCKS_EXCLUDED(lock_);

  bool LockSpan(DiscardableSharedMemoryHeap::Span* span)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UnlockSpan(DiscardableSharedMemoryHeap::Span* span)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UnlockAndReleaseMemory(
      DiscardableMemoryImpl* memory,
      std::unique_ptr<DiscardableSharedMemoryHeap::Span> span)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ReleaseSpan(std::unique_ptr<DiscardableSharedMemoryHeap::Span> span)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  size_t GetBytesAllocatedLocked() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  // TODO(penghuang): Switch to SharedRemote when it starts supporting
  // sync method call.
  std::unique_ptr<mojo::Remote<mojom::DiscardableSharedMemoryManager>>
      manager_mojo_;

  // Holds all locked and unlocked instances which have not yet been purged.
  std::set<raw_ptr<DiscardableMemoryImpl, SetExperimental>> allocated_memory_
      GUARDED_BY(lock_);
  size_t bytes_allocated_limit_for_testing_ = 0;

  // Used in metrics to distinguish in-use consumers from background ones. We
  // initialize this to false to avoid getting any data before we are certain
  // we're in the foreground. This is parallel to what we do in
  // RenderThreadImpl.
  bool foregrounded_ = false;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace discardable_memory

#endif  // COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
