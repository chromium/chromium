// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/capabilities.h"
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

namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {

// Provides a surface that manages its own buffers, backed by GpuMemoryBuffers
// created using CHROMIUM_image. Double/triple buffering is implemented
// internally. Doublebuffering occurs if PageFlipComplete is called before the
// next BindFramebuffer call, otherwise it creates extra buffers.
class VIZ_SERVICE_EXPORT BufferQueue {
 public:
  BufferQueue(gpu::gles2::GLES2Interface* gl,
              gfx::BufferFormat format,
              gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
              gpu::SurfaceHandle surface_handle,
              const gpu::Capabilities& capabilities);
  virtual ~BufferQueue();

  // Returns the texture name of the current buffer and the name of the
  // corresponding stencil buffer. The returned values are 0u if there is no
  // current buffer and one could not be created.
  unsigned GetCurrentBuffer(unsigned* stencil);

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
  void PageFlipComplete();

  // Destroys all the buffers (useful if for some reason, the buffers are no
  // longer presentable).
  void FreeAllSurfaces();

  // Frees all buffers if |size|, |color_space|, or |use_stencil| correspond to
  // a change of state. Otherwise, it's a no-op. Returns true if there was a
  // change of state, false otherwise.
  bool Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool use_stencil);

  uint32_t internal_format() const { return internal_format_; }
  gfx::BufferFormat buffer_format() const { return format_; }
  uint32_t texture_target() const { return texture_target_; }

 private:
  friend class BufferQueueTest;
  friend class AllocatedSurface;

  struct VIZ_SERVICE_EXPORT AllocatedSurface {
    AllocatedSurface(BufferQueue* buffer_queue,
                     std::unique_ptr<gfx::GpuMemoryBuffer> buffer,
                     unsigned texture,
                     unsigned image,
                     unsigned stencil,
                     const gfx::Rect& rect);
    ~AllocatedSurface();
    BufferQueue* const buffer_queue;
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer;
    const unsigned texture;
    const unsigned image;
    const unsigned stencil;
    gfx::Rect damage;  // This is the damage for this frame from the previous.
  };

  void FreeSurfaceResources(AllocatedSurface* surface);

  void UpdateBufferDamage(const gfx::Rect& damage);

  // Return a surface, available to be drawn into.
  std::unique_ptr<AllocatedSurface> GetNextSurface();

  gpu::gles2::GLES2Interface* const gl_;
  gfx::Size size_;
  gfx::ColorSpace color_space_;
  bool use_stencil_ = false;
  size_t allocated_count_;
  uint32_t texture_target_;
  uint32_t internal_format_;
  gfx::BufferFormat format_;
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
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  gpu::SurfaceHandle surface_handle_;

  DISALLOW_COPY_AND_ASSIGN(BufferQueue);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_BUFFER_QUEUE_H_
