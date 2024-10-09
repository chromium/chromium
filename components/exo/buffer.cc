// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "components/exo/frame_sink_resource_manager.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/media_switches.h"
#include "ui/aura/env.h"
#include "ui/color/color_id.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

namespace exo {
namespace {

// The amount of time before we wait for release queries using
// GetQueryObjectuivEXT(GL_QUERY_RESULT_EXT).
const int kWaitForReleaseDelayMs = 500;

constexpr char kBufferInUse[] = "BufferInUse";
const unsigned kDefaultQueryType = GL_COMMANDS_COMPLETED_CHROMIUM;
const bool kDefaultUseZeroCopy = true;
const bool kDefaultIsOverlayCandidate = false;
const bool kDefaultYInvert = false;
const gfx::BufferFormat kDefaultBufferFormat = gfx::BufferFormat::RGBA_8888;
const gfx::Size kDefaultSize = gfx::Size(0, 0);
const gfx::BufferUsage kDefaultBufferUsage = gfx::BufferUsage::GPU_READ;

// Default usage in order to create a mappable shared image and get a
// GpuMemoryBufferHandle from it.
const gpu::SharedImageUsageSet kDefaultMappableSIUsage =
    gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

// Gets the color type of |format| for creating bitmap. If it returns
// SkColorType::kUnknown_SkColorType, it means with this format, this buffer
// contents should not be used to create bitmap.
SkColorType GetColorTypeForBitmapCreation(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      return SkColorType::kRGBA_8888_SkColorType;
    case gfx::BufferFormat::BGRA_8888:
      return SkColorType::kBGRA_8888_SkColorType;
    default:
      // Don't create bitmap for other formats.
      return SkColorType::kUnknown_SkColorType;
  }
}

// Gets the shared image format equivalent of |buffer_format| used for creating
// shared image.
viz::SharedImageFormat GetSharedImageFormat(gfx::BufferFormat buffer_format) {
  viz::SharedImageFormat format;
  switch (buffer_format) {
    case gfx::BufferFormat::BGRA_8888:
      return viz::SinglePlaneFormat::kBGRA_8888;
    case gfx::BufferFormat::R_8:
      return viz::SinglePlaneFormat::kR_8;
    case gfx::BufferFormat::R_16:
      return viz::SinglePlaneFormat::kR_16;
    case gfx::BufferFormat::RG_1616:
      return viz::SinglePlaneFormat::kRG_1616;
    case gfx::BufferFormat::RGBA_4444:
      return viz::SinglePlaneFormat::kRGBA_4444;
    case gfx::BufferFormat::RGBA_8888:
      return viz::SinglePlaneFormat::kRGBA_8888;
    case gfx::BufferFormat::RGBA_F16:
      return viz::SinglePlaneFormat::kRGBA_F16;
    case gfx::BufferFormat::BGR_565:
      return viz::SinglePlaneFormat::kBGR_565;
    case gfx::BufferFormat::RG_88:
      return viz::SinglePlaneFormat::kRG_88;
    case gfx::BufferFormat::RGBX_8888:
      return viz::SinglePlaneFormat::kRGBX_8888;
    case gfx::BufferFormat::BGRX_8888:
      return viz::SinglePlaneFormat::kBGRX_8888;
    case gfx::BufferFormat::RGBA_1010102:
      return viz::SinglePlaneFormat::kRGBA_1010102;
    case gfx::BufferFormat::BGRA_1010102:
      return viz::SinglePlaneFormat::kBGRA_1010102;
    case gfx::BufferFormat::YVU_420:
      format = viz::MultiPlaneFormat::kYV12;
      break;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      format = viz::MultiPlaneFormat::kNV12;
      break;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      format = viz::MultiPlaneFormat::kNV12A;
      break;
    case gfx::BufferFormat::P010:
      format = viz::MultiPlaneFormat::kP010;
      break;
  }
#if BUILDFLAG(IS_CHROMEOS)
  // If format is true multiplanar format, we prefer external sampler on
  // ChromeOS.
  if (format.is_multi_plane()) {
    format.SetPrefersExternalSampler();
  }
#endif
  return format;
}

// Helper to create ClientSharedImage.
gpu::SharedImageInterface* GetSharedImageInterface() {
  ui::ContextFactory* context_factory =
      aura::Env::GetInstance()->context_factory();
  CHECK(context_factory);
  // Note : This can fail if GPU acceleration has been disabled.
  scoped_refptr<viz::RasterContextProvider> context_provider =
      context_factory->SharedMainThreadRasterContextProvider();
  if (!context_provider) {
    DLOG(ERROR) << "Failed to acquire a context provider";
    CHECK(context_provider);
    return nullptr;
  }
  return context_provider->SharedImageInterface();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Buffer::Texture

// Encapsulates the state and logic needed to bind a buffer to a SharedImage.
class Buffer::Texture : public viz::ContextLostObserver {
 public:
  Texture(scoped_refptr<viz::RasterContextProvider> context_provider,
          const gfx::Size& size,
          gfx::ColorSpace color_space,
          gpu::SyncToken& sync_token_out);
  Texture(scoped_refptr<viz::RasterContextProvider> context_provider,
          gfx::GpuMemoryBufferHandle* gpu_memory_buffer_handle,
          const gfx::BufferFormat buffer_format,
          const gfx::Size& size,
          gfx::ColorSpace color_space,
          unsigned query_type,
          base::TimeDelta wait_for_release_time,
          bool is_overlay_candidate,
          gpu::SyncToken& sync_token_out);

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  ~Texture() override;

  // Overridden from viz::ContextLostObserver:
  void OnContextLost() override;

  // Returns true if the RasterInterface context has been lost.
  bool IsLost();

  // Allow texture to be reused after |sync_token| has passed and runs
  // |callback|.
  void Release(base::OnceCallback<void(gfx::GpuFenceHandle)> callback,
               viz::ReturnedResource resource);

  // Updates the contents referenced by |gpu_memory_buffer_handle_| returned by
  // mailbox().
  // Returns a sync token that can be used when accessing the SharedImage from a
  // different context.
  gpu::SyncToken UpdateSharedImage(
      std::unique_ptr<gfx::GpuFence> acquire_fence);

  // Releases the contents referenced by |mailbox_| after |sync_token| has
  // passed and runs |callback| when completed.
  void ReleaseSharedImage(
      base::OnceCallback<void(gfx::GpuFenceHandle)> callback,
      viz::ReturnedResource resource);

  // Copy the contents of texture to |destination| and runs |callback| when
  // completed. Returns a sync token that can be used when accessing texture
  // from a different context.
  gpu::SyncToken CopyTexImage(std::unique_ptr<gfx::GpuFence> acquire_fence,
                              Texture* destination,
                              base::OnceClosure callback);

  // Returns the ClientSharedImage for this texture.
  gpu::ClientSharedImage* shared_image() const { return shared_image_.get(); }

  // Returns the mailbox for this texture.
  gpu::Mailbox mailbox() const { return shared_image_->mailbox(); }

 private:
  void DestroyResources();
  void ReleaseWhenQueryResultIsAvailable(base::OnceClosure callback);
  void Released();
  void ScheduleWaitForRelease(base::TimeDelta delay);
  void WaitForRelease();
  const void* GetBufferId() const;

  // Note that the owning reference to this pointers is ::Buffer which can be
  // destroyed before it when ::Buffer::Texture is destroyed via
  // ::Buffer::Texture::ReleaseSharedImage(). This causes pointer to dangle. But
  // this pointer is safe to dangle as we never access it during
  // ::Buffer::Texture destructor and is also never accessed after the owning
  // object ::Buffer is destroyed.
  const raw_ptr<gfx::GpuMemoryBufferHandle, DisableDanglingPtrDetection>
      gpu_memory_buffer_handle_;
  const gfx::Size size_;
  scoped_refptr<viz::RasterContextProvider> context_provider_;
  const unsigned query_type_;
  unsigned query_id_ = 0;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  base::OnceClosure release_callback_;
  const base::TimeDelta wait_for_release_delay_;
  base::TimeTicks wait_for_release_time_;
  bool wait_for_release_pending_ = false;
  base::WeakPtrFactory<Texture> weak_ptr_factory_{this};
};

Buffer::Texture::Texture(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& size,
    gfx::ColorSpace color_space,
    gpu::SyncToken& sync_token_out)
    : gpu_memory_buffer_handle_(nullptr),
      size_(size),
      context_provider_(std::move(context_provider)),
      query_type_(GL_COMMANDS_COMPLETED_CHROMIUM) {
  gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();

  // These SharedImages are used over the raster interface as both the source
  // and destination of writes. Note that as the browser process raster
  // interface uses RasterImplementation (and not RasterImplementationGLES) as
  // its implementation, GLES2 usage is not needed.
  const gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                         gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
                                         gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  shared_image_ =
      sii->CreateSharedImage({viz::SinglePlaneFormat::kRGBA_8888, size,
                              color_space, usage, gpu::kExoTextureLabelPrefix},
                             gpu::kNullSurfaceHandle);
  CHECK(shared_image_);
  DCHECK(!shared_image_->mailbox().IsZero());
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  sync_token_out = sii->GenUnverifiedSyncToken();
  ri->WaitSyncTokenCHROMIUM(sync_token_out.GetConstData());

  // Provides a notification when |context_provider_| is lost.
  context_provider_->AddObserver(this);
}

Buffer::Texture::Texture(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    gfx::GpuMemoryBufferHandle* gpu_memory_buffer_handle,
    const gfx::BufferFormat buffer_format,
    const gfx::Size& size,
    gfx::ColorSpace color_space,
    unsigned query_type,
    base::TimeDelta wait_for_release_delay,
    bool is_overlay_candidate,
    gpu::SyncToken& sync_token_out)
    : gpu_memory_buffer_handle_(gpu_memory_buffer_handle),
      size_(size),
      context_provider_(std::move(context_provider)),
      query_type_(query_type),
      wait_for_release_delay_(wait_for_release_delay) {
  CHECK(!gpu_memory_buffer_handle_->is_null());

  gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();

  // These SharedImages are used over the raster interface as both the source
  // and destination of writes. Note that as the browser process raster
  // interface uses RasterImplementation (and not RasterImplementationGLES) as
  // its implementation, GLES2 usage is not needed.
  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                   gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
                                   gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  if (is_overlay_candidate) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  shared_image_ =
      sii->CreateSharedImage({GetSharedImageFormat(buffer_format), size_,
                              color_space, usage, gpu::kExoTextureLabelPrefix},
                             gpu_memory_buffer_handle_->Clone());
  CHECK(shared_image_);
  DCHECK(!shared_image_->mailbox().IsZero());
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  sync_token_out = sii->GenUnverifiedSyncToken();
  ri->WaitSyncTokenCHROMIUM(sync_token_out.GetConstData());
  ri->GenQueriesEXT(1, &query_id_);

  // Provides a notification when |context_provider_| is lost.
  context_provider_->AddObserver(this);
}

Buffer::Texture::~Texture() {
  DestroyResources();
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
  }
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

void Buffer::Texture::Release(
    base::OnceCallback<void(gfx::GpuFenceHandle)> callback,
    viz::ReturnedResource resource) {
  if (context_provider_) {
    // Only need to wait on the sync token if we don't have a release fence.
    if (resource.sync_token.HasData() && resource.release_fence.is_null()) {
      gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
      ri->WaitSyncTokenCHROMIUM(resource.sync_token.GetConstData());
    }
  }

  // Run callback as texture can be reused immediately after waiting for sync
  // token.
  std::move(callback).Run(std::move(resource.release_fence));
}

gpu::SyncToken Buffer::Texture::UpdateSharedImage(
    std::unique_ptr<gfx::GpuFence> acquire_fence) {
  gpu::SyncToken sync_token;
  if (context_provider_) {
    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    CHECK(shared_image_);
    // UpdateSharedImage gets called only after |mailbox_| can be reused.
    // A buffer can be reattached to a surface only after it has been returned
    // to wayland clients. We return buffers to clients only after the query
    // |query_type_| is available.
    sii->UpdateSharedImage(gpu::SyncToken(), std::move(acquire_fence),
                           shared_image_->mailbox());
    sync_token = sii->GenUnverifiedSyncToken();
    TRACE_EVENT_ASYNC_STEP_INTO0("exo", kBufferInUse, GetBufferId(), "bound");
  }
  return sync_token;
}

void Buffer::Texture::ReleaseSharedImage(
    base::OnceCallback<void(gfx::GpuFenceHandle)> callback,
    viz::ReturnedResource resource) {
  // Only need to wait on the sync token and query if we don't have a release
  // fence.
  if (context_provider_ && resource.release_fence.is_null()) {
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    if (resource.sync_token.HasData()) {
      ri->WaitSyncTokenCHROMIUM(resource.sync_token.GetConstData());
    }
    ri->BeginQueryEXT(query_type_, query_id_);
    ri->EndQueryEXT(query_type_);
    // Run callback when query result is available (i.e., when all operations
    // on the shared image have completed and it's ready to be reused) if sync
    // token has data and buffer has been used. If buffer was never used then
    // run the callback immediately.
    if (resource.sync_token.HasData()) {
      ReleaseWhenQueryResultIsAvailable(base::BindOnce(
          std::move(callback), /*release_fence=*/gfx::GpuFenceHandle()));
      return;
    }
  }
  std::move(callback).Run(std::move(resource.release_fence));
}

gpu::SyncToken Buffer::Texture::CopyTexImage(
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    Texture* destination,
    base::OnceClosure callback) {
  gpu::SyncToken sync_token;
  if (context_provider_) {
    CHECK(shared_image_);
    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    sii->UpdateSharedImage(gpu::SyncToken(), std::move(acquire_fence),
                           shared_image_->mailbox());
    sync_token = sii->GenUnverifiedSyncToken();

    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    DCHECK_NE(query_id_, 0u);
    ri->BeginQueryEXT(query_type_, query_id_);

    // This function is used only to copy a Texture backed by a GMB to a Texture
    // that is not backed by a GMB and has RGBA_8888 format. The texture target
    // to use for RGBA_8888 on ChromeOS is always GL_TEXTURE_2D.
    ri->CopySharedImage(shared_image_->mailbox(),
                        destination->shared_image_->mailbox(), GL_TEXTURE_2D, 0,
                        0, 0, 0, size_.width(), size_.height(),
                        /*unpack_flip_y=*/false,
                        /*unpack_premultiply_alpha=*/false);
    ri->EndQueryEXT(query_type_);
    // Run callback when query result is available.
    ReleaseWhenQueryResultIsAvailable(std::move(callback));
    // Create and return a sync token that can be used to ensure that the
    // CopySharedImage call is processed before issuing any commands
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
    sii->DestroySharedImage(gpu::SyncToken(), std::move(shared_image_));
  }
}

void Buffer::Texture::ReleaseWhenQueryResultIsAvailable(
    base::OnceClosure callback) {
  DCHECK(context_provider_);
  DCHECK(release_callback_.is_null());
  release_callback_ = std::move(callback);
  wait_for_release_time_ = base::TimeTicks::Now() + wait_for_release_delay_;
  ScheduleWaitForRelease(wait_for_release_delay_);
  TRACE_EVENT_ASYNC_STEP_INTO0("exo", kBufferInUse, GetBufferId(),
                               "pending_query");
  context_provider_->ContextSupport()->SignalQuery(
      query_id_, base::BindOnce(&Buffer::Texture::Released,
                                weak_ptr_factory_.GetWeakPtr()));
}

void Buffer::Texture::Released() {
  if (!release_callback_.is_null()) {
    std::move(release_callback_).Run();
  }
}

void Buffer::Texture::ScheduleWaitForRelease(base::TimeDelta delay) {
  if (wait_for_release_pending_) {
    return;
  }

  wait_for_release_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Buffer::Texture::WaitForRelease,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void Buffer::Texture::WaitForRelease() {
  DCHECK(wait_for_release_pending_);
  wait_for_release_pending_ = false;

  if (release_callback_.is_null()) {
    return;
  }

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

const void* Buffer::Texture::GetBufferId() const {
  return static_cast<const void*>(gpu_memory_buffer_handle_);
}

Buffer::BufferRelease::BufferRelease(
    gfx::GpuFenceHandle release_fence,
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller,
    base::OnceClosure buffer_release_callback)
    : release_fence(std::move(release_fence)),
      controller(std::move(controller)),
      buffer_release_callback(std::move(buffer_release_callback)) {}

Buffer::BufferRelease::~BufferRelease() = default;

Buffer::BufferRelease::BufferRelease(BufferRelease&&) = default;

Buffer::BufferRelease& Buffer::BufferRelease::operator=(BufferRelease&&) =
    default;

////////////////////////////////////////////////////////////////////////////////
// Buffer, public:

Buffer::Buffer()
    : Buffer(gfx::GpuMemoryBufferHandle(),
             kDefaultBufferFormat,
             kDefaultSize,
             kDefaultBufferUsage,
             kDefaultQueryType,
             kDefaultUseZeroCopy,
             kDefaultIsOverlayCandidate,
             kDefaultYInvert) {}

Buffer::Buffer(gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle,
               gfx::BufferFormat buffer_format,
               gfx::Size size,
               gfx::BufferUsage buffer_usage,
               unsigned query_type,
               bool use_zero_copy,
               bool is_overlay_candidate,
               bool y_invert)
    : gpu_memory_buffer_handle_(std::move(gpu_memory_buffer_handle)),
      buffer_format_(buffer_format),
      size_(size),
      buffer_usage_(buffer_usage),
      query_type_(query_type),
      use_zero_copy_(use_zero_copy),
      is_overlay_candidate_(is_overlay_candidate),
      y_invert_(y_invert),
      wait_for_release_delay_(base::Milliseconds(kWaitForReleaseDelayMs)) {}

Buffer::~Buffer() = default;

// static
std::unique_ptr<Buffer> Buffer::CreateBufferFromGMBHandle(
    gfx::GpuMemoryBufferHandle buffer_handle,
    const gfx::Size& buffer_size,
    gfx::BufferFormat buffer_format,
    gfx::BufferUsage buffer_usage,
    unsigned query_type,
    bool use_zero_copy,
    bool is_overlay_candidate,
    bool y_invert) {
  return base::WrapUnique(new Buffer(
      std::move(buffer_handle), buffer_format, buffer_size, buffer_usage,
      query_type, use_zero_copy, is_overlay_candidate, y_invert));
}

// static
std::unique_ptr<Buffer> Buffer::CreateBuffer(
    gfx::Size buffer_size,
    gfx::BufferFormat buffer_format,
    gfx::BufferUsage buffer_usage,
    std::string_view debug_label,
    gpu::SurfaceHandle surface_handle,
    base::WaitableEvent* shutdown_event,
    bool is_overlay_candidate) {
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  auto* sii = GetSharedImageInterface();
  if (sii) {
    // Note that we are creating this mappable shared image only to get a
    // GMBHandle from it and use below to create ::Buffer.
    // TODO(vikassoni) : Once MappableSI is fully launched
    // and we remove legacy code paths, refactor ::Buffer and
    // ::Buffer::Texture to use this MappableSI created below directly in
    // ::Buffer::Texture instead of creating new SI in it.
    // ::Buffer will keep a GMB handle as well as MappableSI when handles
    // comes externally via ::CreateBufferFromGMBHandle whereas only
    // MappableSI for ::CreateBuffer calls. ::Buffer also needs to handle
    // context loss since its using a SI.
    // Currently creating ::Buffer from MappableSI below and then using that
    // ::Buffer to create ::Buffer::Texture does not work well as the ::Buffer
    // does not implement ContextLostObserver like ::Buffer::Texture. Even if
    // ::Buffer does implement ContextLostObserver and destroys the MappableSI
    // correctly, it still needs to recreate it when contexts are recreated.
    shared_image = sii->CreateSharedImage(
        {GetSharedImageFormat(buffer_format), buffer_size, gfx::ColorSpace(),
         kDefaultMappableSIUsage, "ExoBufferCreateBuffer"},
        surface_handle, buffer_usage);
  }
  if (!shared_image) {
    LOG(ERROR) << "Failed to create a mappable shared image.";
    return nullptr;
  }
  std::unique_ptr<Buffer> buffer = base::WrapUnique(
      new Buffer(shared_image->CloneGpuMemoryBufferHandle(), buffer_format,
                 buffer_size, buffer_usage, kDefaultQueryType,
                 kDefaultUseZeroCopy, is_overlay_candidate, kDefaultYInvert));

  // Destroy the |shared_image| as it will no longer be used. Note that the
  // underlying handle is already cloned above and will not be destroyed by
  // destroying the |shared_image|.
  sii->DestroySharedImage(gpu::SyncToken(), std::move(shared_image));
  return buffer;
}

bool Buffer::ProduceTransferableResource(
    FrameSinkResourceManager* resource_manager,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    bool secure_output_only,
    viz::TransferableResource* resource,
    gfx::ColorSpace color_space,
    ProtectedNativePixmapQueryDelegate* protected_native_pixmap_query,
    PerCommitExplicitReleaseCallback per_commit_explicit_release_callback) {
  TRACE_EVENT1("exo", "Buffer::ProduceTransferableResource", "buffer_id",
               GetBufferId());
  DCHECK(attach_count_);
  next_commit_id_++;

  // If textures are lost, destroy them to ensure that we create new ones
  // below.
  if (contents_texture_ && contents_texture_->IsLost()) {
    contents_texture_.reset();
  }
  if (texture_ && texture_->IsLost()) {
    texture_.reset();
  }

  ui::ContextFactory* context_factory =
      aura::Env::GetInstance()->context_factory();
  // Note: This can fail if GPU acceleration has been disabled.
  scoped_refptr<viz::RasterContextProvider> context_provider =
      context_factory->SharedMainThreadRasterContextProvider();
  if (!context_provider) {
    DLOG(WARNING) << "Failed to acquire a context provider";
    resource->id = viz::kInvalidResourceId;
    resource->size = gfx::Size();
    if (per_commit_explicit_release_callback) {
      std::move(per_commit_explicit_release_callback)
          .Run(/*release_fence=*/gfx::GpuFenceHandle());
    }
    return false;
  }

  const bool request_release_fence =
      !per_commit_explicit_release_callback.is_null();
  if (per_commit_explicit_release_callback) {
    pending_explicit_releases_.emplace(
        next_commit_id_, std::move(per_commit_explicit_release_callback));
  }

  resource->id = resource_manager->AllocateResourceId();
  resource->format = viz::SinglePlaneFormat::kRGBA_8888;
  resource->size = GetSize();

  resource->resource_source =
      viz::TransferableResource::ResourceSource::kExoBuffer;

  // Create a new image texture for |gpu_memory_buffer_handle_| if one doesn't
  // already exist. The contents of this buffer are copied to |texture| using a
  // call to CopyTexImage.
  if (!contents_texture_) {
    contents_texture_ = std::make_unique<Texture>(
        context_provider, &gpu_memory_buffer_handle_, buffer_format_, size_,
        color_space, query_type_, wait_for_release_delay_,
        is_overlay_candidate_, resource->mutable_sync_token());
  }
  Texture* contents_texture = contents_texture_.get();

  if (release_contents_callback_.IsCancelled()) {
    TRACE_EVENT_ASYNC_BEGIN1("exo", kBufferInUse, GetBufferId(), "buffer_id",
                             GetBufferId());
  }

  // Cancel pending contents release callback.
  release_contents_callback_.Reset(
      base::BindOnce(&Buffer::ReleaseContents, base::Unretained(this)));

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  // Check if this buffer needs HW protection. This can only happen if we
  // require a secure output.
  if (secure_output_only &&
      protected_buffer_state_ == ProtectedBufferState::UNKNOWN &&
      !gpu_memory_buffer_handle_.is_null() && protected_native_pixmap_query) {
    gfx::GpuMemoryBufferHandle gmb_handle = gpu_memory_buffer_handle_.Clone();
    if (!gmb_handle.native_pixmap_handle.planes.empty()) {
      base::ScopedFD pixmap_handle(HANDLE_EINTR(
          dup(gmb_handle.native_pixmap_handle.planes[0].fd.get())));
      if (pixmap_handle.is_valid()) {
        protected_buffer_state_ = ProtectedBufferState::QUERYING;
        protected_native_pixmap_query->IsProtectedNativePixmapHandle(
            std::move(pixmap_handle),
            base::BindOnce(&Buffer::OnIsProtectedNativePixmapHandle,
                           AsWeakPtr()));
      }
    }
  }
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  // Zero-copy means using the contents texture directly.
  if (use_zero_copy_) {
    // This binds the latest contents of this buffer to |contents_texture|.

    // If there is no acquire fence there is no need to update the shared image.
    // We can sync on the existing sync token if present. Examples of where this
    // can happen is video, where there is no fence provided, or in
    // raster/composite when the fence already signaled at this stage.

    if (acquire_fence && !acquire_fence->GetGpuFenceHandle().is_null()) {
      resource->set_sync_token(
          contents_texture->UpdateSharedImage(std::move(acquire_fence)));
    }
    uint32_t texture_target =
        contents_texture->shared_image()->GetTextureTarget();
    resource->set_mailbox(contents_texture->mailbox());
    resource->set_texture_target(texture_target);
    resource->is_overlay_candidate = is_overlay_candidate_;
    resource->format = GetSharedImageFormat(buffer_format_);

    if (context_provider->ContextCapabilities().chromium_gpu_fence &&
        request_release_fence) {
      resource->synchronization_type =
          viz::TransferableResource::SynchronizationType::kReleaseFence;
    }

    // The contents texture will be released when no longer used by the
    // compositor.
    resource_manager->SetResourceReleaseCallback(
        resource->id,
        base::BindOnce(&Buffer::Texture::ReleaseSharedImage,
                       base::Unretained(contents_texture),
                       base::BindOnce(&Buffer::ReleaseContentsTexture,
                                      AsWeakPtr(), std::move(contents_texture_),
                                      release_contents_callback_.callback(),
                                      next_commit_id_)));
    return true;
  }

  // Create a mailbox texture that we copy the buffer contents to.
  if (!texture_) {
    texture_ =
        std::make_unique<Texture>(context_provider, GetSize(), color_space,
                                  resource->mutable_sync_token());
  }
  Texture* texture = texture_.get();

  // Copy the contents of |contents_texture| to |texture| and produce a
  // texture mailbox from the result in |texture|. The contents texture will
  // be released when copy has completed.
  gpu::SyncToken sync_token = contents_texture->CopyTexImage(
      std::move(acquire_fence), texture,
      base::BindOnce(&Buffer::ReleaseContentsTexture, AsWeakPtr(),
                     std::move(contents_texture_),
                     release_contents_callback_.callback(), next_commit_id_,
                     /*release_fence=*/gfx::GpuFenceHandle()));
  resource->set_mailbox(texture->mailbox());
  resource->set_sync_token(sync_token);
  resource->set_texture_target(GL_TEXTURE_2D);
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

void Buffer::SkipLegacyRelease() {
  legacy_release_skippable_ = true;
}

void Buffer::OnAttach() {
  DLOG_IF(WARNING, attach_count_ && !legacy_release_skippable_)
      << "Reattaching a buffer that is already attached to another surface.";
  TRACE_EVENT2("exo", "Buffer::OnAttach", "buffer_id", GetBufferId(), "count",
               attach_count_);
  ++attach_count_;
}

void Buffer::OnDetach() {
  DCHECK_GT(attach_count_, 0u);
  TRACE_EVENT2("exo", "Buffer::OnAttach", "buffer_id", GetBufferId(), "count",
               attach_count_);
  --attach_count_;

  // Release buffer if no longer attached to a surface and content has been
  // released.
  if (!attach_count_ && release_contents_callback_.IsCancelled()) {
    Release();
  }
}

gfx::Size Buffer::GetSize() const {
  return size_;
}

gfx::BufferFormat Buffer::GetFormat() const {
  return buffer_format_;
}

// TODO(vikassoni): Note that once MappableSI is fully landed, direct use of
// GMBs will go away and clients will end up using either GMBHandle or Mappable
// shared image. Below method will be updated accordingly.
const void* Buffer::GetBufferId() const {
  return static_cast<const void*>(&gpu_memory_buffer_handle_);
}

SkColor4f Buffer::GetColor() const {
  return SkColors::kBlack;
}

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
bool Buffer::NeedsHardwareProtection() {
  // We don't indicate protection is needed in the UNKNOWN state because we have
  // not seen a pixmap yet that could be protected.
  return protected_buffer_state_ == ProtectedBufferState::PROTECTED ||
         protected_buffer_state_ == ProtectedBufferState::QUERYING;
}
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

void Buffer::Release() {
  TRACE_EVENT_ASYNC_END0("exo", kBufferInUse, GetBufferId());

  // Run release callback to notify the client that buffer has been released.
  if (!release_callback_.is_null() && !legacy_release_skippable_) {
    release_callback_.Run();
  }
}

void Buffer::ReleaseTexture(std::unique_ptr<Texture> texture,
                            gfx::GpuFenceHandle release_fence) {
  // Buffer was composited - we should not receive a release fence.
  DCHECK(release_fence.is_null());
  texture_ = std::move(texture);
}

void Buffer::ReleaseContentsTexture(std::unique_ptr<Texture> texture,
                                    base::OnceClosure callback,
                                    uint64_t commit_id,
                                    gfx::GpuFenceHandle release_fence) {
  contents_texture_ = std::move(texture);
  MaybeRunPerCommitRelease(commit_id, std::move(release_fence),
                           std::move(callback));
}

void Buffer::ReleaseContents() {
  TRACE_EVENT1("exo", "Buffer::ReleaseContents", "buffer_id", GetBufferId());

  // Cancel callback to indicate that buffer has been released.
  release_contents_callback_.Cancel();

  if (attach_count_) {
    TRACE_EVENT_ASYNC_STEP_INTO0("exo", kBufferInUse, GetBufferId(),
                                 "attached");
  } else {
    // Release buffer if not attached to surface.
    Release();
  }
}

void Buffer::MaybeRunPerCommitRelease(
    uint64_t commit_id,
    gfx::GpuFenceHandle release_fence,
    base::OnceClosure buffer_release_callback) {
  auto iter = pending_explicit_releases_.find(commit_id);
  if (iter != pending_explicit_releases_.end()) {
    std::move(iter->second).Run(release_fence.Clone());
    pending_explicit_releases_.erase(iter);
  }

  // We are still required to send these wl_buffer.release events even if
  // the client supports explicit synchronization.
  if (!buffer_release_callback) {
    return;
  }

  if (release_fence.is_null() || legacy_release_skippable_) {
    std::move(buffer_release_callback).Run();
  } else {
    // Watching the release fence's fd results in a context switch to the I/O
    // thread. That may steal thread time from other applications, which can
    // do something useful during that time. Moreover, most of the time the
    // fence can have already been signalled. Thus, only watch the fence is
    // readable iff it hasn't been signalled yet.
    base::TimeTicks ticks;
    auto status =
        gfx::GpuFence::GetStatusChangeTime(release_fence.Peek(), &ticks);
    if (status == gfx::GpuFence::kSignaled) {
      std::move(buffer_release_callback).Run();
      return;
    }

    auto controller = base::FileDescriptorWatcher::WatchReadable(
        release_fence.Peek(),
        base::BindRepeating(&Buffer::FenceSignalled, AsWeakPtr(), commit_id));
    buffer_releases_.emplace(
        commit_id,
        BufferRelease(std::move(release_fence), std::move(controller),
                      std::move(buffer_release_callback)));
  }
}

void Buffer::FenceSignalled(uint64_t commit_id) {
  auto iter = buffer_releases_.find(commit_id);
  CHECK(iter != buffer_releases_.end(), base::NotFatalUntil::M130);
  std::move(iter->second.buffer_release_callback).Run();
  buffer_releases_.erase(iter);
}

SkBitmap Buffer::CreateBitmap() {
  SkBitmap bitmap;
  SkColorType color_type = GetColorTypeForBitmapCreation(GetFormat());
  if (color_type == SkColorType::kUnknown_SkColorType) {
    return bitmap;
  }

  auto* sii = GetSharedImageInterface();
  if (gpu_memory_buffer_handle_.is_null() || !sii) {
    return bitmap;
  }

  // We only need to create this shared image in order to Map the
  // |gpu_memory_buffer_handle_| to cpu visible memory.
  auto shared_image = sii->CreateSharedImage(
      {GetSharedImageFormat(buffer_format_), size_, gfx::ColorSpace(),
       kDefaultMappableSIUsage, "ExoBufferCreateBitmap"},
      gpu::kNullSurfaceHandle, buffer_usage_,
      gpu_memory_buffer_handle_.Clone());

  auto mapping = shared_image->Map();
  if (!mapping) {
    LOG(ERROR) << "Failed to map MappableSI.";
    return bitmap;
  }

  gfx::Size size = GetSize();
  SkImageInfo image_info = SkImageInfo::Make(size.width(), size.height(),
                                             color_type, kPremul_SkAlphaType);

  bitmap.allocPixels(image_info);
  bitmap.writePixels(mapping->GetSkPixmapForPlane(0, image_info));
  bitmap.setImmutable();
  mapping.reset();

  // Destroy this shared image as we no longer need it.
  sii->DestroySharedImage(gpu::SyncToken(), std::move(shared_image));
  return bitmap;
}

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
void Buffer::OnIsProtectedNativePixmapHandle(bool is_protected) {
  protected_buffer_state_ = is_protected ? ProtectedBufferState::PROTECTED
                                         : ProtectedBufferState::UNPROTECTED;
}
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

base::WeakPtr<Buffer> Buffer::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

SolidColorBuffer::SolidColorBuffer(const SkColor4f& color,
                                   const gfx::Size& size)
    : color_(color), size_(size) {
  SkipLegacyRelease();
}

SolidColorBuffer::~SolidColorBuffer() = default;

bool SolidColorBuffer::ProduceTransferableResource(
    FrameSinkResourceManager* resource_manager,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    bool secure_output_only,
    viz::TransferableResource* resource,
    gfx::ColorSpace color_space,
    ProtectedNativePixmapQueryDelegate* protected_native_pixmap_query,
    PerCommitExplicitReleaseCallback per_commit_explicit_release_callback) {
  if (per_commit_explicit_release_callback) {
    std::move(per_commit_explicit_release_callback)
        .Run(/*release_fence=*/gfx::GpuFenceHandle());
  }
  return false;
}

SkColor4f SolidColorBuffer::GetColor() const {
  return color_;
}

gfx::Size SolidColorBuffer::GetSize() const {
  return size_;
}

base::WeakPtr<Buffer> SolidColorBuffer::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace exo
