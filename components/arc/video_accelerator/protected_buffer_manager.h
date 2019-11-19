// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/files/scoped_file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"

namespace arc {
class ProtectedBufferAllocator;

class ProtectedBufferManager
    : public base::RefCountedThreadSafe<ProtectedBufferManager> {
 public:
  ProtectedBufferManager();

  // Creates ProtectedBufferAllocatorImpl and return it as
  // unique_ptr<ProtectedBufferAllocator>.
  // The created PBA would call the function |protected_buffer_manager|.
  static std::unique_ptr<ProtectedBufferAllocator>
  CreateProtectedBufferAllocator(
      scoped_refptr<ProtectedBufferManager> protected_buffer_manager);

  // Return a duplicated PlatformSharedMemoryRegion associated with the
  // |dummy_fd|, if one exists, or an invalid handle otherwise. The client is
  // responsible for closing the handle after use.
  base::subtle::PlatformSharedMemoryRegion GetProtectedSharedMemoryRegionFor(
      base::ScopedFD dummy_fd);

  // Return a duplicated NativePixmapHandle associated with the |dummy_fd|,
  // if one exists, or an empty handle otherwise.
  // The client is responsible for closing the handle after use.
  gfx::NativePixmapHandle GetProtectedNativePixmapHandleFor(
      base::ScopedFD dummy_fd);

  // Return a protected NativePixmap for a dummy |handle|, if one exists, or
  // nullptr otherwise.
  scoped_refptr<gfx::NativePixmap> GetProtectedNativePixmapFor(
      const gfx::NativePixmapHandle& handle);

 private:
  // Used internally to maintain the association between the dummy handle and
  // the underlying buffer.
  class ProtectedBuffer;
  class ProtectedSharedMemory;
  class ProtectedNativePixmap;
  class ProtectedBufferAllocatorImpl;

  // Be friend with ProtectedBufferAllocatorImpl so that private functions can
  // be called in ProtectedBufferAllocatorImpl.
  friend class ProtectedBufferAllocatorImpl;

  friend class base::RefCountedThreadSafe<ProtectedBufferManager>;
  // Destructor must be private for base::RefCounted class.
  ~ProtectedBufferManager();

  // Returns whether the number of active protected buffer allocators is less
  // than the predetermined threshold (kMaxConcurrentProtectedBufferAllocators).
  // This also returns current available allocator id through |allocator_id|.
  bool GetAllocatorId(uint64_t* const allocator_id);

  // Allocates a ProtectedSharedMemory buffer of |size| bytes, to be referred to
  // via |dummy_fd| as the dummy handle.
  // |allocator_id| is the allocator id of the caller.
  // Returns whether allocation is successful.
  bool AllocateProtectedSharedMemory(uint64_t allocator_id,
                                     base::ScopedFD dummy_fd,
                                     size_t size);

  // Allocates a ProtectedNativePixmap of |format| and |size|, to be referred to
  // via |dummy_fd| as the dummy handle.
  // |allocator_id| is the allocator id of the caller.
  // Returns whether allocation is successful.
  bool AllocateProtectedNativePixmap(uint64_t allocator_id,
                                     base::ScopedFD dummy_fd,
                                     gfx::BufferFormat format,
                                     const gfx::Size& size);

  // Releases reference to ProtectedSharedMemory or ProtectedNativePixmap
  // referred via |dummy_fd|. |allocator_id| is the allocator id of the caller.
  void ReleaseProtectedBuffer(uint64_t allocator_id, base::ScopedFD dummy_fd);

  // Releases all the references of protected buffers which is allocated by PBA
  // whose allocator id is |allocator_id|.
  void ReleaseAllProtectedBuffers(uint64_t allocator_id);

  // Imports the |dummy_fd| as a NativePixmap. This returns a unique |id|,
  // which is guaranteed to be the same for all future imports of any fd
  // referring to the buffer to which |dummy_fd| refers to, regardless of
  // whether it is the same fd as the original one, or not, for the lifetime
  // of the buffer.
  //
  // This allows us to have an unambiguous mapping from any fd referring to
  // the same memory buffer to the same unique id.
  //
  // Returns nullptr on failure, in which case the returned id is not valid.
  scoped_refptr<gfx::NativePixmap> ImportDummyFd(base::ScopedFD dummy_fd,
                                                 uint32_t* id) const;

  // Removes an entry for given |id| from buffer_map_.
  void RemoveEntry(uint32_t id) EXCLUSIVE_LOCKS_REQUIRED(buffer_map_lock_);

  // Returns whether a protected buffer whose unique id is |id| can be
  // allocated by PBA whose allocator id is |allocator_id|.
  bool CanAllocateFor(uint64_t allocator_id, uint32_t id)
      EXCLUSIVE_LOCKS_REQUIRED(buffer_map_lock_);

  // A map of unique ids to the ProtectedBuffers associated with them.
  using ProtectedBufferMap =
      std::map<uint32_t, std::unique_ptr<ProtectedBuffer>>;
  ProtectedBufferMap buffer_map_ GUARDED_BY(buffer_map_lock_);
  // A map of allocator ids to the unique ids of ProtectedBuffers allocated by
  // the allocator with the allocator id. The size is equal to the number of
  // active protected buffer allocators.
  std::map<uint64_t, std::set<uint32_t>> allocator_to_buffers_map_
      GUARDED_BY(buffer_map_lock_);
  uint64_t next_protected_buffer_allocator_id_ GUARDED_BY(buffer_map_lock_);

  base::Lock buffer_map_lock_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedBufferManager);
};
}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_H_
