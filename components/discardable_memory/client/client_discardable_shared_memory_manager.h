// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
#define COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_

#include <stddef.h>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/lock.h"
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
      public base::trace_event::MemoryDumpProvider {
 public:
  ClientDiscardableSharedMemoryManager(
      mojo::PendingRemote<mojom::DiscardableSharedMemoryManager> manager,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~ClientDiscardableSharedMemoryManager() override;

  // Overridden from base::DiscardableMemoryAllocator:
  std::unique_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Release memory and associated resources that have been purged.
  void ReleaseFreeMemory() override;

  bool LockSpan(DiscardableSharedMemoryHeap::Span* span);
  void UnlockSpan(DiscardableSharedMemoryHeap::Span* span);
  void ReleaseSpan(std::unique_ptr<DiscardableSharedMemoryHeap::Span> span);

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      DiscardableSharedMemoryHeap::Span* span,
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const;

  struct Statistics {
    size_t total_size;
    size_t freelist_size;
  };

  size_t GetBytesAllocated() const override;

 private:
  std::unique_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size, int32_t id);
  void AllocateOnIO(size_t size,
                    int32_t id,
                    base::UnsafeSharedMemoryRegion* region,
                    base::ScopedClosureRunner closure_runner);
  void AllocateCompletedOnIO(base::UnsafeSharedMemoryRegion* region,
                             base::ScopedClosureRunner closure_runner,
                             base::UnsafeSharedMemoryRegion ret_region);

  void DeletedDiscardableSharedMemory(int32_t id);
  void MemoryUsageChanged(size_t new_bytes_allocated,
                          size_t new_bytes_free) const;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  // TODO(penghuang): Switch to SharedRemote when it starts supporting
  // sync method call.
  std::unique_ptr<mojo::Remote<mojom::DiscardableSharedMemoryManager>>
      manager_mojo_;

  mutable base::Lock lock_;
  std::unique_ptr<DiscardableSharedMemoryHeap> heap_;

  DISALLOW_COPY_AND_ASSIGN(ClientDiscardableSharedMemoryManager);
};

}  // namespace discardable_memory

#endif  // COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
