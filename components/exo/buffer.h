// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_BUFFER_H_
#define COMPONENTS_EXO_BUFFER_H_

#include <memory>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {

class FrameSinkResourceManager;

// This class provides the content for a Surface. The mechanism by which a
// client provides and updates the contents is the responsibility of the client
// and not defined as part of this class.
class Buffer : public base::SupportsWeakPtr<Buffer> {
 public:
  explicit Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer);
  Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
         unsigned texture_target,
         unsigned query_type,
         bool use_zero_copy,
         bool is_overlay_candidate,
         bool y_invert);
  ~Buffer();

  const gfx::GpuMemoryBuffer* gfx_buffer() const {
    return gpu_memory_buffer_.get();
  }

  // Set the callback to run when the buffer is no longer used by the
  // compositor. The client is free to re-use or destroy this buffer and
  // its backing storage after this has been called.
  void set_release_callback(const base::RepeatingClosure& release_callback) {
    release_callback_ = release_callback;
  }

  // Returns if this buffer's contents are vertically inverted.
  bool y_invert() const { return y_invert_; }

  // This function can be used to acquire a texture mailbox for the contents of
  // buffer. Returns a release callback on success. The release callback should
  // be called before a new texture mailbox can be acquired unless
  // |non_client_usage| is true.
  bool ProduceTransferableResource(FrameSinkResourceManager* resource_manager,
                                   std::unique_ptr<gfx::GpuFence> acquire_fence,
                                   bool secure_output_only,
                                   viz::TransferableResource* resource);

  // This should be called when the buffer is attached to a Surface.
  void OnAttach();

  // This should be called when the buffer is detached from a surface.
  void OnDetach();

  // Returns the size of the buffer.
  gfx::Size GetSize() const;

  // Returns the format of the buffer.
  gfx::BufferFormat GetFormat() const;

  // Set the amount of time to wait for buffer release.
  void set_wait_for_release_delay_for_testing(
      base::TimeDelta wait_for_release_delay) {
    wait_for_release_delay_ = wait_for_release_delay;
  }

 private:
  class Texture;

  // This should be called when buffer is released and will notify the
  // client that buffer has been released.
  void Release();

  // This is used by ProduceTransferableResource() to produce a release callback
  // that releases a texture so it can be destroyed or reused.
  void ReleaseTexture(std::unique_ptr<Texture> texture);

  // This is used by ProduceTransferableResource() to produce a release callback
  // that releases the buffer contents referenced by a texture before the
  // texture is destroyed or reused.
  void ReleaseContentsTexture(std::unique_ptr<Texture> texture,
                              base::OnceClosure callback);

  // Notifies the client that buffer has been released if no longer attached
  // to a surface.
  void ReleaseContents();

  // The GPU memory buffer that contains the contents of this buffer.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

  // Texture target that must be used when creating a texture for buffer.
  const unsigned texture_target_;

  // Query type that must be used when releasing buffer from a texture.
  const unsigned query_type_;

  // True if zero copy is used when producing a texture mailbox for buffer.
  const bool use_zero_copy_;

  // True if this buffer is an overlay candidate.
  const bool is_overlay_candidate_;

  // True if buffer content is vertically inverted.
  const bool y_invert_;

  // This keeps track of how many Surfaces the buffer is attached to.
  unsigned attach_count_ = 0;

  // The last used texture. ProduceTransferableResource() will use this
  // instead of creating a new texture when possible.
  std::unique_ptr<Texture> texture_;

  // The last used contents texture. ProduceTransferableResource() will use this
  // instead of creating a new texture when possible.
  std::unique_ptr<Texture> contents_texture_;

  // The client release callback.
  base::RepeatingClosure release_callback_;

  // Cancelable release contents callback. This is set when a release callback
  // is pending.
  base::CancelableOnceClosure release_contents_callback_;

  // The amount of time to wait for buffer release.
  base::TimeDelta wait_for_release_delay_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_BUFFER_H_
