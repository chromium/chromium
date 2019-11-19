// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/exo/frame_sink_resource_manager.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

// The amount of time before we wait for release queries using
// GetQueryObjectuivEXT(GL_QUERY_RESULT_EXT).
const int kWaitForReleaseDelayMs = 500;

constexpr char kBufferInUse[] = "BufferInUse";

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Buffer::Texture

// Encapsulates the state and logic needed to bind a buffer to a SharedImage.
class Buffer::Texture : public viz::ContextLostObserver {
 public:
  Texture(scoped_refptr<viz::RasterContextProvider> context_provider,
          const gfx::Size& size);
  Texture(scoped_refptr<viz::RasterContextProvider> context_provider,
          gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
          gfx::GpuMemoryBuffer* gpu_memory_buffer,
          unsigned texture_target,
          unsigned query_type,
          base::TimeDelta wait_for_release_time);
  ~Texture() override;

  // Overridden from viz::ContextLostObserver:
  void OnContextLost() override;

  // Returns true if the RasterInterface context has been lost.
  bool IsLost();

  // Allow texture to be reused after |sync_token| has passed and runs
  // |callback|.
  void Release(base::OnceClosure callback,
               const gpu::SyncToken& sync_token,
               bool is_lost);

  // Updates the contents referenced by |gpu_memory_buffer_| returned by
  // mailbox().
  // Returns a sync token that can be used when accessing the SharedImage from a
  // different context.
  gpu::SyncToken UpdateSharedImage(
      std::unique_ptr<gfx::GpuFence> acquire_fence);

  // Releases the contents referenced by |mailbox_| after |sync_token| has
  // passed and runs |callback| when completed.
  void ReleaseSharedImage(base::OnceClosure callback,
                          const gpu::SyncToken& sync_token,
                          bool is_lost);

  // Copy the contents of texture to |destination| and runs |callback| when
  // completed. Returns a sync token that can be used when accessing texture
  // from a different context.
  gpu::SyncToken CopyTexImage(std::unique_ptr<gfx::GpuFence> acquire_fence,
                              Texture* destination,
                              base::OnceClosure callback);

  // Returns the mailbox for this texture.
  gpu::Mailbox mailbox() const { return mailbox_; }

 private:
  void DestroyResources();
  void ReleaseWhenQueryResultIsAvailable(base::OnceClosure callback);
  void Released();
  void ScheduleWaitForRelease(base::TimeDelta delay);
  void WaitForRelease();

  gfx::GpuMemoryBuffer* const gpu_memory_buffer_;
  const gfx::Size size_;
  scoped_refptr<viz::RasterContextProvider> context_provider_;
  const unsigned texture_target_;
  const unsigned query_type_;
  unsigned query_id_ = 0;
  gpu::Mailbox mailbox_;
  base::OnceClosure release_callback_;
  const base::TimeDelta wait_for_release_delay_;
  base::TimeTicks wait_for_release_time_;
  bool wait_for_release_pending_ = false;
  base::WeakPtrFactory<Texture> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Texture);
};

Buffer::Texture::Texture(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& size)
    : gpu_memory_buffer_(nullptr),
      size_(size),
      context_provider_(std::move(context_provider)),
      texture_target_(GL_TEXTURE_2D),
      query_type_(GL_COMMANDS_COMPLETED_CHROMIUM) {
  gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
  const uint32_t usage =
      gpu::SHARED_IMAGE_USAGE_RASTER | gpu::SHARED_IMAGE_USAGE_DISPLAY;

  mailbox_ = sii->CreateSharedImage(viz::ResourceFormat::RGBA_8888, size,
                                    gfx::ColorSpace(), usage);
  DCHECK(!mailbox_.IsZero());
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  // Provides a notification when |context_provider_| is lost.
  context_provider_->AddObserver(this);
}

Buffer::Texture::Texture(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    unsigned texture_target,
    unsigned query_type,
    base::TimeDelta wait_for_release_delay)
    : gpu_memory_buffer_(gpu_memory_buffer),
      size_(gpu_memory_buffer->GetSize()),
      context_provider_(std::move(context_provider)),
      texture_target_(texture_target),
      query_type_(query_type),
      wait_for_release_delay_(wait_for_release_delay) {
  gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
  const uint32_t usage = gpu::SHARED_IMAGE_USAGE_RASTER |
                         gpu::SHARED_IMAGE_USAGE_DISPLAY |
                         gpu::SHARED_IMAGE_USAGE_SCANOUT;

  mailbox_ = sii->CreateSharedImage(
      gpu_memory_buffer_, gpu_memory_buffer_manager, gfx::ColorSpace(), usage);
  DCHECK(!mailbox_.IsZero());
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  ri->GenQueriesEXT(1, &query_id_);

  // Provides a notification when |context_provider_| is lost.
  context_provider_->AddObserver(this);
}

Buffer::Texture::~Texture() {
  DestroyResources();
  if (context_provider_)
    context_provider_->RemoveObserver(this);
}

void Buffer::Texture::OnContextLost() {
  DestroyResources();
  context_provider_->RemoveObserver(this);
  context_provider_.reset();
}

bool Buffer::Texture::IsLost() {
  if (context_provider_) {
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    return ri->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
  }
  return true;
}

void Buffer::Texture::Release(base::OnceClosure callback,
                              const gpu::SyncToken& sync_token,
                              bool is_lost) {
  if (context_provider_) {
    if (sync_token.HasData()) {
      gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
      ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    }
  }

  // Run callback as texture can be reused immediately after waiting for sync
  // token.
  std::move(callback).Run();
}

gpu::SyncToken Buffer::Texture::UpdateSharedImage(
    std::unique_ptr<gfx::GpuFence> acquire_fence) {
  gpu::SyncToken sync_token;
  if (context_provider_) {
    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    DCHECK(!mailbox_.IsZero());
    // UpdateSharedImage gets called only after |mailbox_| can be reused.
    // A buffer can be reattached to a surface only after it has been returned
    // to wayland clients. We return buffers to clients only after the query
    // |query_type_| is available.
    sii->UpdateSharedImage(gpu::SyncToken(), std::move(acquire_fence),
                           mailbox_);
    sync_token = sii->GenUnverifiedSyncToken();
    TRACE_EVENT_ASYNC_STEP_INTO0("exo", kBufferInUse, gpu_memory_buffer_,
                                 "bound");
  }
  return sync_token;
}

void Buffer::Texture::ReleaseSharedImage(base::OnceClosure callback,
                                         const gpu::SyncToken& sync_token,
                                         bool is_lost) {
  if (context_provider_) {
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    if (sync_token.HasData())
      ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    ri->BeginQueryEXT(query_type_, query_id_);
    ri->EndQueryEXT(query_type_);
    // Run callback when query result is available (i.e., when all operations on
    // the shared image have completed and it's ready to be reused) if sync
    // token has data and buffer has been used. If buffer was never used then
    // run the callback immediately.
    if (sync_token.HasData()) {
      ReleaseWhenQueryResultIsAvailable(std::move(callback));
      return;
    }
  }
  std::move(callback).Run();
}

gpu::SyncToken Buffer::Texture::CopyTexImage(
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    Texture* destination,
    base::OnceClosure callback) {
  gpu::SyncToken sync_token;
  if (context_provider_) {
    DCHECK(!mailbox_.IsZero());
    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    sii->UpdateSharedImage(gpu::SyncToken(), std::move(acquire_fence),
                           mailbox_);
    sync_token = sii->GenUnverifiedSyncToken();

    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    DCHECK_NE(query_id_, 0u);
    ri->BeginQueryEXT(query_type_, query_id_);
    ri->CopySubTexture(mailbox_, destination->mailbox_,
                       destination->texture_target_, 0, 0, 0, 0, size_.width(),
                       size_.height());
    ri->EndQueryEXT(query_type_);
    // Run callback when query result is available.
    ReleaseWhenQueryResultIsAvailable(std::move(callback));
    // Create and return a sync token that can be used to ensure that the
    // CopySubTexture call is processed before issuing any commands
    // that will read from the target texture on a different context.
    ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  }
  return sync_token;
}

void Buffer::Texture::DestroyResources() {
  if (context_provider_) {
    if (query_id_) {
      gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
      ri->DeleteQueriesEXT(1, &query_id_);
      query_id_ = 0;
    }
    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    sii->DestroySharedImage(gpu::SyncToken(), mailbox_);
  }
}

void Buffer::Texture::ReleaseWhenQueryResultIsAvailable(
    base::OnceClosure callback) {
  DCHECK(context_provider_);
  DCHECK(release_callback_.is_null());
  release_callback_ = std::move(callback);
  wait_for_release_time_ = base::TimeTicks::Now() + wait_for_release_delay_;
  ScheduleWaitForRelease(wait_for_release_delay_);
  TRACE_EVENT_ASYNC_STEP_INTO0("exo", kBufferInUse, gpu_memory_buffer_,
                               "pending_query");
  context_provider_->ContextSupport()->SignalQuery(
      query_id_, base::BindOnce(&Buffer::Texture::Released,
                                weak_ptr_factory_.GetWeakPtr()));
}

void Buffer::Texture::Released() {
  if (!release_callback_.is_null())
    std::move(release_callback_).Run();
}

void Buffer::Texture::ScheduleWaitForRelease(base::TimeDelta delay) {
  if (wait_for_release_pending_)
    return;

  wait_for_release_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Buffer::Texture::WaitForRelease,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void Buffer::Texture::WaitForRelease() {
  DCHECK(wait_for_release_pending_);
  wait_for_release_pending_ = false;

  if (release_callback_.is_null())
    return;

  base::TimeTicks current_time = base::TimeTicks::Now();
  if (current_time < wait_for_release_time_) {
    ScheduleWaitForRelease(wait_for_release_time_ - current_time);
    return;
  }

  base::OnceClosure callback = std::move(release_callback_);

  if (context_provider_) {
    TRACE_EVENT0("exo", "Buffer::Texture::WaitForQueryResult");

    // We need to wait for the result to be available. Getting the result of
    // the query implies waiting for it to become available. The actual result
    // is unimportant and also not well defined.
    unsigned result = 0;
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    ri->GetQueryObjectuivEXT(query_id_, GL_QUERY_RESULT_EXT, &result);
  }

  std::move(callback).Run();
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, public:

Buffer::Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer)
    : Buffer(std::move(gpu_memory_buffer),
             GL_TEXTURE_2D /* texture_target */,
             GL_COMMANDS_COMPLETED_CHROMIUM /* query_type */,
             true /* use_zero_copy */,
             false /* is_overlay_candidate */,
             false /* y_invert */) {}

Buffer::Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
               unsigned texture_target,
               unsigned query_type,
               bool use_zero_copy,
               bool is_overlay_candidate,
               bool y_invert)
    : gpu_memory_buffer_(std::move(gpu_memory_buffer)),
      texture_target_(texture_target),
      query_type_(query_type),
      use_zero_copy_(use_zero_copy),
      is_overlay_candidate_(is_overlay_candidate),
      y_invert_(y_invert),
      wait_for_release_delay_(
          base::TimeDelta::FromMilliseconds(kWaitForReleaseDelayMs)) {}

Buffer::~Buffer() {}

bool Buffer::ProduceTransferableResource(
    FrameSinkResourceManager* resource_manager,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    bool secure_output_only,
    viz::TransferableResource* resource) {
  TRACE_EVENT1("exo", "Buffer::ProduceTransferableResource", "buffer_id",
               gfx_buffer());
  DCHECK(attach_count_);

  // If textures are lost, destroy them to ensure that we create new ones below.
  if (contents_texture_ && contents_texture_->IsLost())
    contents_texture_.reset();
  if (texture_ && texture_->IsLost())
    texture_.reset();

  ui::ContextFactory* context_factory =
      aura::Env::GetInstance()->context_factory();
  // Note: This can fail if GPU acceleration has been disabled.
  scoped_refptr<viz::RasterContextProvider> context_provider =
      context_factory->SharedMainThreadRasterContextProvider();
  if (!context_provider) {
    DLOG(WARNING) << "Failed to acquire a context provider";
    resource->id = 0;
    resource->size = gfx::Size();
    return false;
  }

  resource->id = resource_manager->AllocateResourceId();
  resource->format = viz::RGBA_8888;
  resource->filter = GL_LINEAR;
  resource->size = gpu_memory_buffer_->GetSize();

  // Create a new image texture for |gpu_memory_buffer_| with |texture_target_|
  // if one doesn't already exist. The contents of this buffer are copied to
  // |texture| using a call to CopyTexImage.
  if (!contents_texture_) {
    contents_texture_ = std::make_unique<Texture>(
        context_provider, context_factory->GetGpuMemoryBufferManager(),
        gpu_memory_buffer_.get(), texture_target_, query_type_,
        wait_for_release_delay_);
  }
  Texture* contents_texture = contents_texture_.get();

  if (release_contents_callback_.IsCancelled())
    TRACE_EVENT_ASYNC_BEGIN1("exo", kBufferInUse, gpu_memory_buffer_.get(),
                             "buffer_id", gfx_buffer());

  // Cancel pending contents release callback.
  release_contents_callback_.Reset(
      base::BindOnce(&Buffer::ReleaseContents, base::Unretained(this)));

  // Zero-copy means using the contents texture directly.
  if (use_zero_copy_) {
    // This binds the latest contents of this buffer to |contents_texture|.
    gpu::SyncToken sync_token =
        contents_texture->UpdateSharedImage(std::move(acquire_fence));
    resource->mailbox_holder = gpu::MailboxHolder(contents_texture->mailbox(),
                                                  sync_token, texture_target_);
    resource->is_overlay_candidate = is_overlay_candidate_;
    resource->format = viz::GetResourceFormat(gpu_memory_buffer_->GetFormat());

    // The contents texture will be released when no longer used by the
    // compositor.
    resource_manager->SetResourceReleaseCallback(
        resource->id,
        base::BindOnce(&Buffer::Texture::ReleaseSharedImage,
                       base::Unretained(contents_texture),
                       base::BindOnce(&Buffer::ReleaseContentsTexture,
                                      AsWeakPtr(), std::move(contents_texture_),
                                      release_contents_callback_.callback())));
    return true;
  }

  // Create a mailbox texture that we copy the buffer contents to.
  if (!texture_) {
    texture_ = std::make_unique<Texture>(context_provider,
                                         gpu_memory_buffer_->GetSize());
  }
  Texture* texture = texture_.get();

  // Copy the contents of |contents_texture| to |texture| and produce a
  // texture mailbox from the result in |texture|. The contents texture will
  // be released when copy has completed.
  gpu::SyncToken sync_token = contents_texture->CopyTexImage(
      std::move(acquire_fence), texture,
      base::BindOnce(&Buffer::ReleaseContentsTexture, AsWeakPtr(),
                     std::move(contents_texture_),
                     release_contents_callback_.callback()));
  resource->mailbox_holder =
      gpu::MailboxHolder(texture->mailbox(), sync_token, GL_TEXTURE_2D);
  resource->is_overlay_candidate = false;

  // The mailbox texture will be released when no longer used by the
  // compositor.
  resource_manager->SetResourceReleaseCallback(
      resource->id,
      base::BindOnce(&Buffer::Texture::Release, base::Unretained(texture),
                     base::BindOnce(&Buffer::ReleaseTexture, AsWeakPtr(),
                                    std::move(texture_))));
  return true;
}

void Buffer::OnAttach() {
  DLOG_IF(WARNING, attach_count_)
      << "Reattaching a buffer that is already attached to another surface.";
  TRACE_EVENT2("exo", "Buffer::OnAttach", "buffer_id", gfx_buffer(), "count",
               attach_count_);
  ++attach_count_;
}

void Buffer::OnDetach() {
  DCHECK_GT(attach_count_, 0u);
  TRACE_EVENT2("exo", "Buffer::OnAttach", "buffer_id", gfx_buffer(), "count",
               attach_count_);
  --attach_count_;

  // Release buffer if no longer attached to a surface and content has been
  // released.
  if (!attach_count_ && release_contents_callback_.IsCancelled())
    Release();
}

gfx::Size Buffer::GetSize() const {
  return gpu_memory_buffer_->GetSize();
}

gfx::BufferFormat Buffer::GetFormat() const {
  return gpu_memory_buffer_->GetFormat();
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

void Buffer::Release() {
  TRACE_EVENT_ASYNC_END0("exo", kBufferInUse, gpu_memory_buffer_.get());

  // Run release callback to notify the client that buffer has been released.
  if (!release_callback_.is_null())
    release_callback_.Run();
}

void Buffer::ReleaseTexture(std::unique_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

void Buffer::ReleaseContentsTexture(std::unique_ptr<Texture> texture,
                                    base::OnceClosure callback) {
  contents_texture_ = std::move(texture);
  std::move(callback).Run();
}

void Buffer::ReleaseContents() {
  TRACE_EVENT1("exo", "Buffer::ReleaseContents", "buffer_id", gfx_buffer());

  // Cancel callback to indicate that buffer has been released.
  release_contents_callback_.Cancel();

  if (attach_count_) {
    TRACE_EVENT_ASYNC_STEP_INTO0("exo", kBufferInUse, gpu_memory_buffer_.get(),
                                 "attached");
  } else {
    // Release buffer if not attached to surface.
    Release();
  }
}

}  // namespace exo
