// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class SkiaOutputSurface;

// Encapsulates a queue of buffers for compositing backed by SharedImages.
// Double/triple/N-buffering is configured by specifying |number_of_buffers| at
// construction, or by calling EnsureMinNumberOfBuffers().
class VIZ_SERVICE_EXPORT BufferQueue {
 public:
  // Creates a BufferQueue that allocates SharedImage buffers using |sii|.
  // Buffers are not allocated until Reshape() is called. |number_of_buffers|
  // specifies the number of buffers that will be allocated, and can be
  // increased by calling EnsureMinNumberOfBuffers() when
  // |supports_dynamic_frame_buffer_allocation| capability is true.
  BufferQueue(SkiaOutputSurface* skia_output_surface,
              gpu::SurfaceHandle surface_handle,
              size_t number_of_buffers,
              bool is_protected = false);

  BufferQueue(const BufferQueue&) = delete;
  BufferQueue& operator=(const BufferQueue&) = delete;

  ~BufferQueue();

  // Returns the SharedImage backed by the current buffer (i.e., the render
  // target for compositing).
  gpu::Mailbox GetCurrentBuffer();

  // Returns a mailbox to be used for overlay testing. This will be the last
  // swapped buffer if one exists, or another buffer in the queue if not. This
  // will return a zero-mailbox if DestroyBuffers() has been called and buffers
  // have not been recreated since.
  gpu::Mailbox GetLastSwappedBuffer();

  // Returns a rectangle whose contents may have changed since the current
  // buffer was last submitted and needs to be redrawn. For partial swap,
  // only the contents outside this rectangle can be considered valid and do not
  // need to be redrawn.
  gfx::Rect CurrentBufferDamage() const;

  // Called by the user of this object to indicate that the buffer currently
  // marked for drawing should be moved to the list of in-flight buffers.
  // |damage| represents the rectangle containing the damaged area since the
  // last SwapBuffers.
  void SwapBuffers(const gfx::Rect& damage);

  // Called by the user of this object to indicate that a previous request to
  // swap buffers has completed. This allows us to correctly keep track of the
  // state of the buffers: the buffer currently marked as being displayed will
  // now marked as available, and the next buffer marked as in-flight will now
  // be marked as displayed.
  void SwapBuffersComplete();

  // Called when SwapBuffers is skipped this frame. Damages allocated buffers,
  // but does not advance |in_flight_buffers_| or |current_buffer_|. We don't
  // clear the damage on |current_buffer_| because it hasn't been displayed yet.
  // SwapBuffersComplete() must not be called for skipped swap.
  void SwapBuffersSkipped(const gfx::Rect& damage);

  // If |size| or |color_space| correspond to a change of state, frees all
  // the buffers and reallocatess |number_of_buffers_| buffers. Otherwise, it's
  // a no-op. Returns true if there was a change of state, false otherwise.
  bool Reshape(const gfx::Size& size,
               const gfx::ColorSpace& color_space,
               SharedImageFormat format);

  // Sets the number of frame buffers to use when
  // |supports_dynamic_frame_buffer_allocation| is true, and allocates those
  // buffers if necessary. If |n| <= |number_of_buffers_| this is a no-op.
  void EnsureMinNumberOfBuffers(size_t n);

  // Free all buffers and allocate |number_of_buffers_| new ones.
  // Note: SwapBuffersComplete() calls are still expected for all current
  // in-flight buffers, but they've been free'd so they won't be moved to
  // |available_buffers_|.
  void RecreateBuffers();

  // Destroys all allocated buffers. This should be used when the renderer knows
  // these buffers will no longer be needed, e.g. when delegating to the system
  // compositor.
  // Buffers will only be recreated the next time GetCurrentBuffer() is called.
  // NOTE: This should not be used on platforms that use buffers for
  // overlay testing because GetLastSwappedBuffer() will not recreate the
  // buffers.
  void DestroyBuffers();

  // Indicates buffer contents can be purged, aka their contents deleted if
  // memory is needed. For each completed swap one buffer will be marked
  // purgeable.
  //
  // NOTE: This should only be used when buffers are not currently needed, eg.
  // when delegating to system compositor, and if the platform support purgeable
  // shared images.
  void SetBuffersPurgeable();

 private:
  friend class BufferQueueTest;
  friend class BufferQueueMockedSharedImageInterfaceTest;
  FRIEND_TEST_ALL_PREFIXES(BufferQueueTest, AllocateFails);
  FRIEND_TEST_ALL_PREFIXES(BufferQueueMockedSharedImageInterfaceTest,
                           AllocateFails);

  struct VIZ_SERVICE_EXPORT AllocatedBuffer {
    AllocatedBuffer(const gpu::Mailbox& mailbox, const gfx::Rect& rect);
    ~AllocatedBuffer();

    bool purgeable = false;
    gpu::Mailbox mailbox;
    gfx::Rect damage;  // This is the damage for this frame from the previous.
  };

  // Frees all buffers that have been allocated, and destroys their shared
  // images.
  void FreeAllBuffers();

  // Free |buffer| and destroy its shared image.
  void FreeBuffer(std::unique_ptr<AllocatedBuffer> buffer);

  // Sets `buffer`s shared image as `purgeable` and returns true if the value
  // changed.
  bool SetBufferPurgeable(AllocatedBuffer& buffer, bool purgeable);

  // Unions |damage| to all allocated buffers except |current_buffer_| which
  // hasn't been displayed yet.
  void UpdateBufferDamage(const gfx::Rect& damage);

  // Allocates |n| buffers and pushes them into |available_buffers_|.
  void AllocateBuffers(size_t n);

  // Return a buffer that is available to be drawn into.
  std::unique_ptr<AllocatedBuffer> GetNextBuffer();

  // If |buffers_destroyed_| = true, this will create |number_of_buffers_|
  // buffers with the settings last set by Reshape().
  void RecreateBuffersIfDestroyed();

  // Used to create and destroy shared images.
  const raw_ptr<SkiaOutputSurface> skia_output_surface_;
  // Used when creating shared images.
  gpu::SurfaceHandle surface_handle_;
  // The number of buffers that should be allocated when Reshape() is called.
  size_t number_of_buffers_ = 0;

  // The size of all allocated buffers.
  gfx::Size size_;
  // The color space of all allocated buffers.
  gfx::ColorSpace color_space_;
  // The format of all allocated buffers. The |format_| is optional to prevent
  // use of uninitialized values.
  std::optional<SharedImageFormat> format_;

  // This buffer is currently bound. This may be nullptr if no buffer has
  // been bound.
  std::unique_ptr<AllocatedBuffer> current_buffer_;
  // The buffer currently on the screen, if any.
  std::unique_ptr<AllocatedBuffer> displayed_buffer_;
  // These are free for use, and are not nullptr.
  base::circular_deque<std::unique_ptr<AllocatedBuffer>> available_buffers_;
  // These have been swapped but are not displayed yet. Entries of this deque
  // may be nullptr, if they represent frames that have been destroyed, or
  // frames where SwapBuffers() was called without calling GetCurrentBuffer().
  base::circular_deque<std::unique_ptr<AllocatedBuffer>> in_flight_buffers_;

  // When the buffers are not being used due to delegated compositing.
  bool buffers_can_be_purged_ = false;

  // Whether the buffers have been destroyed and are not yet recreated. If true,
  // don't allocate buffers when you normally would. They will be recreated on
  // demand the next time GetNextBuffer() is called.
  bool buffers_destroyed_ = false;
  // Started when DestroyBuffers() destroys all buffers, and reported the next
  // time RecreateBuffersIfDestroyed() is called. The timer will be reset after
  // reporting.
  // Used to see how often we destroy buffers and recreate them very soon, which
  // we want to be rare.
  std::optional<base::ElapsedTimer> destroyed_timer_;
  // Whether or not to allocate these buffers as protected buffers.
  bool is_protected_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_
