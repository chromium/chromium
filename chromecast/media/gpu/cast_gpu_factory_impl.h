// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_IMPL_H_
#define CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "chromecast/media/gpu/cast_gpu_factory.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
class UnguessableToken;
}  // namespace base

namespace gpu {
class GpuChannelHost;
}  // namespace gpu

namespace viz {
class Gpu;
}  // namespace viz

namespace chromecast {

class RemoteInterfaces;

class CastGpuFactoryImpl : public CastGpuFactory,
                           public ::media::GpuVideoAcceleratorFactories {
 public:
  CastGpuFactoryImpl(
      scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner,
      RemoteInterfaces* browser_services);
  ~CastGpuFactoryImpl() override;
  CastGpuFactoryImpl(const CastGpuFactoryImpl&) = delete;
  CastGpuFactoryImpl& operator=(const CastGpuFactoryImpl&) = delete;

 private:
  // CastGpuFactory implementation:
  scoped_refptr<viz::ContextProviderCommandBuffer> CreateOpenGLContextProvider()
      override;
  std::unique_ptr<::media::VideoDecoder> CreateVideoDecoder() override;
  std::unique_ptr<::media::VideoEncodeAccelerator> CreateVideoEncoder()
      override;

  // media::GpuVideoAcceleratorFactories implementation.
  bool IsGpuVideoDecodeAcceleratorEnabled() override;
  bool IsGpuVideoEncodeAcceleratorEnabled() override;
  base::UnguessableToken GetChannelToken() override;
  int32_t GetCommandBufferRouteId() override;
  ::media::GpuVideoAcceleratorFactories::Supported IsDecoderConfigSupported(
      const ::media::VideoDecoderConfig& config) override;
  bool IsDecoderSupportKnown() override;
  void NotifyDecoderSupportKnown(base::OnceClosure) override;
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      ::media::MediaLog* media_log,
      ::media::RequestOverlayInfoCB request_overlay_info_cb) override;
  std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
  GetVideoEncodeAcceleratorSupportedProfiles() override;
  bool IsEncoderSupportKnown() override;
  void NotifyEncoderSupportKnown(base::OnceClosure) override;
  std::unique_ptr<::media::VideoEncodeAccelerator>
  CreateVideoEncodeAccelerator() override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const override;
  unsigned ImageTextureTarget(gfx::BufferFormat format) override;
  OutputFormat VideoFrameOutputFormat(
      ::media::VideoPixelFormat pixel_format) override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() override;
  base::UnsafeSharedMemoryRegion CreateSharedMemoryRegion(size_t size) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override;
  viz::RasterContextProvider* GetMediaContextProvider() override;
  void SetRenderingColorSpace(const gfx::ColorSpace& color_space) override;
  const gfx::ColorSpace& GetRenderingColorSpace() const override;

  // Setup GPU context.
  void SetupContext();

  // Verify the context provider connection. Return true if connection
  // is lost.
  bool CheckContextLost();

  base::UnguessableToken channel_token_;
  base::Thread gpu_io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner_;
  std::unique_ptr<viz::Gpu> gpu_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  mojo::Remote<media::mojom::VideoEncodeAcceleratorProvider> vea_provider_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;

  // The default color space is invalid color space.
  gfx::ColorSpace rendering_color_space_;

  std::unique_ptr<::media::MediaLog> media_log_;
  RemoteInterfaces* const browser_services_;
};

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_IMPL_H_
