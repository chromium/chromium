// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/gpu/cast_gpu_factory_impl.h"

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/mojo/remote_interfaces.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_info.h"
#include "media/base/media_util.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace chromecast {
namespace {

void OnRequestOverlayInfo(bool decoder_requires_restart_for_overlay,
                          ::media::ProvideOverlayInfoCB overlay_info_cb) {
  // Android overlays are not supported.
  if (overlay_info_cb)
    overlay_info_cb.Run(::media::OverlayInfo());
}

}  // namespace

// static.
std::unique_ptr<CastGpuFactory> CastGpuFactory::Create(
    scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner,
    RemoteInterfaces* browser_services) {
  return std::make_unique<CastGpuFactoryImpl>(std::move(mojo_task_runner),
                                              browser_services);
}

CastGpuFactoryImpl::CastGpuFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner,
    RemoteInterfaces* browser_services)
    : gpu_io_thread_("CastGpuIo"),
      mojo_task_runner_(std::move(mojo_task_runner)),
      media_log_(std::make_unique<::media::NullMediaLog>()),
      browser_services_(browser_services) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  DCHECK(browser_services_);

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  options.thread_type = base::ThreadType::kDisplayCritical;
  gpu_io_thread_.StartWithOptions(std::move(options));

  mojo::PendingRemote<viz::mojom::Gpu> remote_gpu;
  browser_services_->Bind(remote_gpu.InitWithNewPipeAndPassReceiver());
  gpu_ = viz::Gpu::Create(std::move(remote_gpu), gpu_io_thread_.task_runner());
  DCHECK(gpu_);

  // Perform SetupContext asynchronously as the thread (e.g.,
  // CastRendererVirtualCamera) that's handling |browser_services_| GetInterface
  // request could be blocking waiting for this construction to completed, this
  // will allow that thread to be unblocked.
  mojo_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&CastGpuFactoryImpl::SetupContext,
                                             base::Unretained(this)));
}

CastGpuFactoryImpl::~CastGpuFactoryImpl() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
}

scoped_refptr<viz::ContextProviderCommandBuffer>
CastGpuFactoryImpl::CreateOpenGLContextProvider() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.enable_gles2_interface = true;
  attributes.enable_raster_interface = false;
  attributes.enable_oop_rasterization = false;
  attributes.context_type = gpu::CONTEXT_TYPE_OPENGLES3;
  attributes.gpu_preference = gl::GpuPreference::kHighPerformance;
  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      gpu_channel_host_, gpu_->gpu_memory_buffer_manager(), 0 /* stream ID */,
      gpu::SchedulingPriority::kHigh, gpu::kNullSurfaceHandle,
      GURL("chrome://gpu/opengl"), false /* automatic_flushes */,
      false /* support_locking */, false /* support_grcontext */,
      gpu::SharedMemoryLimits::ForMailboxContext(), attributes,
      viz::command_buffer_metrics::ContextType::WEBGL);
}

std::unique_ptr<::media::VideoDecoder>
CastGpuFactoryImpl::CreateVideoDecoder() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  return CreateVideoDecoder(media_log_.get(),
                            base::BindRepeating(&OnRequestOverlayInfo));
}

std::unique_ptr<::media::VideoEncodeAccelerator>
CastGpuFactoryImpl::CreateVideoEncoder() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  return CreateVideoEncodeAccelerator();
}

// Return whether GPU decoding is enabled.
bool CastGpuFactoryImpl::IsGpuVideoDecodeAcceleratorEnabled() {
  return true;
}

// Return whether GPU encoding is enabled.
bool CastGpuFactoryImpl::IsGpuVideoEncodeAcceleratorEnabled() {
  return true;
}

// Return the channel token, or an empty token if the channel is unusable.
base::UnguessableToken CastGpuFactoryImpl::GetChannelToken() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  if (CheckContextLost()) {
    return base::UnguessableToken();
  }

  return channel_token_;
}

// Returns the |route_id| of the command buffer, or 0 if there is none.
int32_t CastGpuFactoryImpl::GetCommandBufferRouteId() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  if (CheckContextLost()) {
    return 0;
  }
  return context_provider_->GetCommandBufferProxy()->route_id();
}

// Return true if |config| is potentially supported by a decoder created with
// CreateVideoDecoder().
::media::GpuVideoAcceleratorFactories::Supported
CastGpuFactoryImpl::IsDecoderConfigSupported(
    const ::media::VideoDecoderConfig& config) {
  if (config.codec() == ::media::VideoCodec::kH264) {
    return Supported::kTrue;
  }
  return Supported::kFalse;
}

bool CastGpuFactoryImpl::IsDecoderSupportKnown() {
  return true;
}

void CastGpuFactoryImpl::NotifyDecoderSupportKnown(base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

std::unique_ptr<::media::VideoDecoder> CastGpuFactoryImpl::CreateVideoDecoder(
    ::media::MediaLog* media_log,
    ::media::RequestOverlayInfoCB request_overlay_info_cb) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  DCHECK(media_interface_factory_.is_bound());

  if (CheckContextLost()) {
    return nullptr;
  }

  mojo::PendingRemote<media::mojom::VideoDecoder> video_decoder;
  media_interface_factory_->CreateVideoDecoder(
      video_decoder.InitWithNewPipeAndPassReceiver());
  return std::make_unique<::media::MojoVideoDecoder>(
      mojo_task_runner_, this, media_log, std::move(video_decoder),
      request_overlay_info_cb, gfx::ColorSpace::CreateSRGB());
}

std::optional<::media::VideoEncodeAccelerator::SupportedProfiles>
CastGpuFactoryImpl::GetVideoEncodeAcceleratorSupportedProfiles() {
  return ::media::VideoEncodeAccelerator::SupportedProfiles();
}

bool CastGpuFactoryImpl::IsEncoderSupportKnown() {
  return true;
}

void CastGpuFactoryImpl::NotifyEncoderSupportKnown(base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

std::unique_ptr<::media::VideoEncodeAccelerator>
CastGpuFactoryImpl::CreateVideoEncodeAccelerator() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  if (CheckContextLost()) {
    return nullptr;
  }
  if (!vea_provider_) {
    return nullptr;
  }

  mojo::PendingRemote<::media::mojom::VideoEncodeAccelerator> vea;
  vea_provider_->CreateVideoEncodeAccelerator(
      vea.InitWithNewPipeAndPassReceiver());
  return std::make_unique<::media::VideoEncodeAccelerator>(
      std::move(vea),
      ::media::GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
          gpu_channel_host_->gpu_info()
              .video_encode_accelerator_supported_profiles));
}

std::unique_ptr<gfx::GpuMemoryBuffer> CastGpuFactoryImpl::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return nullptr;
}

bool CastGpuFactoryImpl::ShouldUseGpuMemoryBuffersForVideoFrames(
    bool for_media_stream) const {
  return false;
}

unsigned CastGpuFactoryImpl::ImageTextureTarget(gfx::BufferFormat format) {
  return 0;
}

media::GpuVideoAcceleratorFactories::OutputFormat
CastGpuFactoryImpl::VideoFrameOutputFormat(
    ::media::VideoPixelFormat pixel_format) {
  return ::media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
}

gpu::SharedImageInterface* CastGpuFactoryImpl::SharedImageInterface() {
  return nullptr;
}

gpu::GpuMemoryBufferManager* CastGpuFactoryImpl::GpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
}

base::UnsafeSharedMemoryRegion CastGpuFactoryImpl::CreateSharedMemoryRegion(
    size_t size) {
  return base::UnsafeSharedMemoryRegion();
}

scoped_refptr<base::SequencedTaskRunner> CastGpuFactoryImpl::GetTaskRunner() {
  return nullptr;
}

viz::RasterContextProvider* CastGpuFactoryImpl::GetMediaContextProvider() {
  return nullptr;
}

void CastGpuFactoryImpl::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {}

const gfx::ColorSpace& CastGpuFactoryImpl::GetRenderingColorSpace() const {
  return rendering_color_space_;
}

void CastGpuFactoryImpl::SetupContext() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << __func__;
  gpu_channel_host_ = gpu_->EstablishGpuChannelSync();
  if (!gpu_channel_host_) {
    LOG(ERROR) << "Failed to obtained GPU channel host";
    return;
  }

  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.enable_gles2_interface = true;
  attributes.enable_raster_interface = false;
  attributes.enable_oop_rasterization = false;
  context_provider_ = base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      gpu_channel_host_, gpu_->gpu_memory_buffer_manager(), 0 /* stream ID */,
      gpu::SchedulingPriority::kHigh, gpu::kNullSurfaceHandle,
      GURL("chrome://gpu/CastVideoAcceleratorFactory"),
      false /* automatic_flushes */, false /* support_locking */,
      gpu::SharedMemoryLimits::ForMailboxContext(), attributes,
      viz::command_buffer_metrics::ContextType::MEDIA);
  DCHECK(context_provider_);

  if (context_provider_->BindToCurrentSequence() !=
      gpu::ContextResult::kSuccess) {
    LOG(ERROR) << "Failed to bind ContextProvider to current thread";
    context_provider_ = nullptr;
  }

  // Get the channel token for the current connection.
  context_provider_->GetCommandBufferProxy()->GetGpuChannel().GetChannelToken(
      &channel_token_);

  gpu_->CreateVideoEncodeAcceleratorProvider(
      vea_provider_.BindNewPipeAndPassReceiver());
  DCHECK(vea_provider_);

  DCHECK(browser_services_);
  browser_services_->BindNewPipe(&media_interface_factory_);
  DCHECK(media_interface_factory_);
}

bool CastGpuFactoryImpl::CheckContextLost() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  if (!context_provider_ ||
      context_provider_->ContextGL()->GetGraphicsResetStatusKHR() !=
          GL_NO_ERROR) {
    LOG(ERROR) << "ContextProvider no longer connected";
    context_provider_ = nullptr;
    // Re-setup GPU context.
    SetupContext();
  }
  return !context_provider_;
}

}  // namespace chromecast
