// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
class GpuMemoryBufferManager;
class SharedImageInterface;
struct SyncToken;
}  // namespace gpu

namespace viz {

// Encapsulates a queue of buffers for compositing backed by SharedImages (in
// turn backed by GpuMemoryBuffers). Double/triple buffering is implemented
// internally. Double buffering occurs if PageFlipComplete is called before the
// next BindFramebuffer call, otherwise it creates extra buffers.
//
// SetSyncTokenProvider() must be called prior to using the BufferQueue. The
// reason the SyncTokenProvider is not passed in the constructor is testing:
// this allows us to create a mock BufferQueue that can be injected into a
// GLOutputSurfaceBufferQueue. The surface can then set itself as the
// SyncTokenProvider and fully own the BufferQueue thus guaranteeing that the
// queue's SyncTokenProvider outlives the queue.
class VIZ_SERVICE_EXPORT BufferQueue {
 public:
  // A BufferQueue uses a SyncTokenProvider to get sync tokens that ensure
  // operations on the buffers done by the BufferQueue client are synchronized
  // with respect to other work.
  //
  // TODO(crbug.com/958670): extend this abstraction to allow both fences and
  // sync tokens.
  class SyncTokenProvider {
   public:
    SyncTokenProvider() = default;
    virtual ~SyncTokenProvider() = default;
    virtual gpu::SyncToken GenSyncToken() = 0;
  };

  // Creates a BufferQueue that allocates buffers using
  // |gpu_memory_buffer_manager| and associates them with SharedImages using
  // |sii|.
  BufferQueue(gpu::SharedImageInterface* sii,
              gpu::SurfaceHandle surface_handle);
  virtual ~BufferQueue();

  // Sets the provider of sync tokens that the BufferQueue needs to ensure
  // operations on a SharedImage are ordered correctly with respect to the
  // operations issued by the client of the BufferQueue. |sync_token_provider|
  // is not used after the BufferQueue is destroyed.
  virtual void SetSyncTokenProvider(SyncTokenProvider* sync_token_provider);

  // Returns the SharedImage backed by the current buffer (i.e., the render
  // target for compositing). A zeroed mailbox is returned if there is no
  // current buffer and one could not be created. The caller needs to wait on
  // *|creation_sync_token| if non-empty before consuming the mailbox.
  virtual gpu::Mailbox GetCurrentBuffer(gpu::SyncToken* creation_sync_token);

  // Returns a rectangle whose contents may have changed since the current
  // buffer was last submitted and needs to be redrawn. For partial swap,
  // only the contents outside this rectangle can be considered valid and do not
  // need to be redrawn.
  virtual gfx::Rect CurrentBufferDamage() const;

  // Called by the user of this object to indicate that the buffer currently
  // marked for drawing should be moved to the list of in-flight buffers.
  // |damage| represents the rectangle containing the damaged area since the
  // last SwapBuffers.
  virtual void SwapBuffers(const gfx::Rect& damage);

  // Called by the user of this object to indicate that a previous request to
  // swap buffers has completed. This allows us to correctly keep track of the
  // state of the buffers: the buffer currently marked as being displayed will
  // now marked as available, and the next buffer marked as in-flight will now
  // be marked as displayed.
  virtual void PageFlipComplete();

  // Requests a sync token from the SyncTokenProvider passed in the constructor
  // and frees all buffers after that sync token has passed.
  virtual void FreeAllSurfaces();

  // If |size| or |color_space| correspond to a change of state, requests a sync
  // token from the SyncTokenProvider passed in the constructor and frees all
  // the buffers after that sync token passes. Otherwise, it's a no-op. Returns
  // true if there was a change of state, false otherwise.
  virtual bool Reshape(const gfx::Size& size,
                       const gfx::ColorSpace& color_space,
                       gfx::BufferFormat format);

  gfx::BufferFormat buffer_format() const { return *format_; }
  void SetMaxBuffers(size_t max);

 private:
  friend class BufferQueueTest;
  friend class BufferQueueMockedSharedImageInterfaceTest;
  FRIEND_TEST_ALL_PREFIXES(BufferQueueTest, AllocateFails);
  FRIEND_TEST_ALL_PREFIXES(BufferQueueMockedSharedImageInterfaceTest,
                           AllocateFails);

  // TODO(andrescj): consider renaming this to AllocatedBuffer because 'surface'
  // is an overloaded term (also problematic in the unit tests).
  struct VIZ_SERVICE_EXPORT AllocatedSurface {
    AllocatedSurface(const gpu::Mailbox& mailbox, const gfx::Rect& rect);
    ~AllocatedSurface();

    // TODO(crbug.com/958670): if we can have a CreateSharedImage() that takes a
    // SurfaceHandle, we don't have to keep track of |buffer|.
    gpu::Mailbox mailbox;
    gfx::Rect damage;  // This is the damage for this frame from the previous.
  };

  void FreeSurface(std::unique_ptr<AllocatedSurface> surface,
                   const gpu::SyncToken& sync_token);

  void UpdateBufferDamage(const gfx::Rect& damage);

  // Return a buffer that is available to be drawn into or nullptr if there is
  // no available buffer and one cannot be created. If a new buffer is created
  // *|creation_sync_token| is set to a sync token that the client must wait on
  // before using the buffer.
  std::unique_ptr<AllocatedSurface> GetNextSurface(
      gpu::SyncToken* creation_sync_token);

  gpu::SharedImageInterface* const sii_;
  gfx::Size size_;
  gfx::ColorSpace color_space_;

  // We don't want to allow anything more than triple buffering by default.
  size_t max_buffers_ = 3U;
  size_t allocated_count_;
  // The |format_| is optional to prevent use of uninitialized values.
  base::Optional<gfx::BufferFormat> format_;
  // This surface is currently bound. This may be nullptr if no surface has
  // been bound, or if allocation failed at bind.
  std::unique_ptr<AllocatedSurface> current_surface_;
  // The surface currently on the screen, if any.
  std::unique_ptr<AllocatedSurface> displayed_surface_;
  // These are free for use, and are not nullptr.
  std::vector<std::unique_ptr<AllocatedSurface>> available_surfaces_;
  // These have been swapped but are not displayed yet. Entries of this deque
  // may be nullptr, if they represent frames that have been destroyed.
  base::circular_deque<std::unique_ptr<AllocatedSurface>> in_flight_surfaces_;
  gpu::SurfaceHandle surface_handle_;
  SyncTokenProvider* sync_token_provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BufferQueue);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_
