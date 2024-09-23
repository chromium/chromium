// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_BUFFER_H_
#define COMPONENTS_EXO_BUFFER_H_

#include <memory>
#include <string_view>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/exo/protected_native_pixmap_query_delegate.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/ipc/common/surface_handle.h"
#include "media/media_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {

class FrameSinkResourceManager;

// This class provides the content for a Surface. The mechanism by which a
// client provides and updates the contents is the responsibility of the client
// and not defined as part of this class.
class Buffer {
 public:
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  virtual ~Buffer();

  // Clients can use this method to create Buffer using GMBHandles. This is
  // required to move away clients from using GMB directly as a part of
  // MappableSI work.
  static std::unique_ptr<Buffer> CreateBufferFromGMBHandle(
      gfx::GpuMemoryBufferHandle buffer_handle,
      const gfx::Size& buffer_size,
      gfx::BufferFormat buffer_format,
      gfx::BufferUsage buffer_usage,
      unsigned query_type,
      bool use_zero_copy,
      bool is_overlay_candidate,
      bool y_invert);

  static std::unique_ptr<Buffer> CreateBuffer(
      gfx::Size buffer_size,
      gfx::BufferFormat buffer_format,
      gfx::BufferUsage buffer_usage,
      std::string_view debug_label,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event,
      bool is_overlay_candidate = false);

  // Set the callback to run when the buffer is no longer used by the
  // compositor. The client is free to re-use or destroy this buffer and
  // its backing storage after this has been called.
  void set_release_callback(const base::RepeatingClosure& release_callback) {
    release_callback_ = release_callback;
  }

  // The client does not need release_callback_ to notify buffer usage.
  void SkipLegacyRelease();

  // Returns if this buffer's contents are vertically inverted.
  bool y_invert() const { return y_invert_; }

  // This function can be used to acquire a texture mailbox for the contents of
  // buffer. |release_callback| will be called when the contents of the buffer
  // are no longer required.
  using PerCommitExplicitReleaseCallback =
      base::OnceCallback<void(gfx::GpuFenceHandle)>;
  virtual bool ProduceTransferableResource(
      FrameSinkResourceManager* resource_manager,
      std::unique_ptr<gfx::GpuFence> acquire_fence,
      bool secure_output_only,
      viz::TransferableResource* resource,
      gfx::ColorSpace color_space,
      ProtectedNativePixmapQueryDelegate* protected_native_pixmap_query,
      PerCommitExplicitReleaseCallback per_commit_explicit_release_callback);

  // This should be called when the buffer is attached to a Surface.
  void OnAttach();

  // This should be called when the buffer is detached from a surface.
  void OnDetach();

  // Returns the size of the buffer.
  virtual gfx::Size GetSize() const;

  // Returns the format of the buffer.
  gfx::BufferFormat GetFormat() const;

  // Returns the |gpu_memory_buffer_| pointer to be used as id. It can also be
  // used as a bool to identify if |gpu_memory_buffer_| is null or not.
  const void* GetBufferId() const;

  // The default color to be used should transferable resource production fail.
  virtual SkColor4f GetColor() const;

  // Creates a SkBitmap object from |gpu_memory_buffer_|.
  SkBitmap CreateBitmap();

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  // Returns true if the underlying buffer is hardware protected. This should
  // only be checked if the corresponding surface requires secure output,
  // otherwise it will yield false positives.
  bool NeedsHardwareProtection();
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  // Set the amount of time to wait for buffer release.
  void set_wait_for_release_delay_for_testing(
      base::TimeDelta wait_for_release_delay) {
    wait_for_release_delay_ = wait_for_release_delay;
  }

  virtual base::WeakPtr<Buffer> AsWeakPtr();

 protected:
  // Currently only derived class access this constructor.
  Buffer();

 private:
  // TODO(vikassoni): Once MappableSI is fully landed, these clients do not need
  // to access the Buffer constructors. So it should be removed from the friend
  // list.
  friend class Display;
  friend class SharedMemory;

  class Texture;

  Buffer(gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle,
         gfx::BufferFormat buffer_format,
         gfx::Size size,
         gfx::BufferUsage buffer_usage,
         unsigned query_type,
         bool use_zero_copy,
         bool is_overlay_candidate,
         bool y_invert);

  struct BufferRelease {
    BufferRelease(
        gfx::GpuFenceHandle release_fence,
        std::unique_ptr<base::FileDescriptorWatcher::Controller> controller,
        base::OnceClosure buffer_release_callback);
    ~BufferRelease();

    BufferRelease(const BufferRelease&) = delete;
    BufferRelease& operator=(const BufferRelease&) = delete;

    BufferRelease(BufferRelease&&);
    BufferRelease& operator=(BufferRelease&&);

    // |release_fence| must be kept above |controller| to keep the file
    // descriptor valid during destruction.
    gfx::GpuFenceHandle release_fence;
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;
    base::OnceClosure buffer_release_callback;
  };

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  // For ARC protected content support this tracks the state of the
  // asynchronous query to determine if the GMB is using a protected buffer or
  // not.
  enum class ProtectedBufferState { UNKNOWN, QUERYING, PROTECTED, UNPROTECTED };
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  // This should be called when buffer is released and will notify the
  // client that buffer has been released.
  void Release();

  // This is used by ProduceTransferableResource() to produce a release callback
  // that releases a texture so it can be destroyed or reused.
  void ReleaseTexture(std::unique_ptr<Texture> texture,
                      gfx::GpuFenceHandle release_fence);

  // This is used by ProduceTransferableResource() to produce a release callback
  // that releases the buffer contents referenced by a texture before the
  // texture is destroyed or reused.
  void ReleaseContentsTexture(std::unique_ptr<Texture> texture,
                              base::OnceClosure callback,
                              uint64_t commit_id,
                              gfx::GpuFenceHandle release_fence);

  // Notifies the client that buffer has been released if no longer attached to
  // a surface.
  void ReleaseContents();

  void MaybeRunPerCommitRelease(uint64_t commit_id,
                                gfx::GpuFenceHandle release_fence,
                                base::OnceClosure buffer_release_callback);

  void FenceSignalled(uint64_t commit_id);

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  void OnIsProtectedNativePixmapHandle(bool is_protected);
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  // Contains the content of this buffer instead of |gpu_memory_buffer_| when
  // MappableSI is enabled.
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle_;
  const gfx::BufferFormat buffer_format_;
  const gfx::Size size_;
  gfx::BufferUsage buffer_usage_;

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

  // Because viz can release buffers out of order, it's necessary to map
  // releases to specific commits. Identify commits via a incrementing counter.
  uint64_t next_commit_id_ = 0;

  // Maps commit count to the callback to call when we receive a release from
  // viz.
  base::flat_map<uint64_t, PerCommitExplicitReleaseCallback>
      pending_explicit_releases_;

  // Maps commit count to information required to send regular buffer releases.
  // Even if we send explicit synchronization release information, Wayland
  // protocol requires us to send regular buffer release events.
  base::flat_map<uint64_t, BufferRelease> buffer_releases_;

  bool legacy_release_skippable_ = false;

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  ProtectedBufferState protected_buffer_state_ = ProtectedBufferState::UNKNOWN;
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  base::WeakPtrFactory<Buffer> weak_ptr_factory_{this};
};

class SolidColorBuffer : public Buffer {
 public:
  SolidColorBuffer(const SkColor4f& color, const gfx::Size& size);
  SolidColorBuffer(const SolidColorBuffer& buffer) = delete;
  SolidColorBuffer& operator=(const SolidColorBuffer&) = delete;
  ~SolidColorBuffer() override;

  SkColor4f GetColor() const override;
  gfx::Size GetSize() const override;
  bool ProduceTransferableResource(
      FrameSinkResourceManager* resource_manager,
      std::unique_ptr<gfx::GpuFence> acquire_fence,
      bool secure_output_only,
      viz::TransferableResource* resource,
      gfx::ColorSpace color_space,
      ProtectedNativePixmapQueryDelegate* protected_native_pixmap_query,
      PerCommitExplicitReleaseCallback per_commit_explicit_release_callback)
      override;

  base::WeakPtr<Buffer> AsWeakPtr() override;

 private:
  SkColor4f color_;
  gfx::Size size_;

  base::WeakPtrFactory<SolidColorBuffer> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_BUFFER_H_
