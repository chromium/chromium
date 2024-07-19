// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GMB_VIDEO_FRAME_POOL_CONTEXT_PROVIDER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GMB_VIDEO_FRAME_POOL_CONTEXT_PROVIDER_IMPL_H_

#include <memory>

#include "components/viz/service/frame_sinks/gmb_video_frame_pool_context_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace viz {

class GpuServiceImpl;
class InProcessGpuMemoryBufferManager;

class VIZ_SERVICE_EXPORT GmbVideoFramePoolContextProviderImpl
    : public GmbVideoFramePoolContextProvider {
 public:
  explicit GmbVideoFramePoolContextProviderImpl(
      GpuServiceImpl* gpu_service,
      InProcessGpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  GmbVideoFramePoolContextProviderImpl(
      const GmbVideoFramePoolContextProviderImpl& other) = delete;
  GmbVideoFramePoolContextProviderImpl& operator=(
      const GmbVideoFramePoolContextProviderImpl& other) = delete;

  ~GmbVideoFramePoolContextProviderImpl() override;

  std::unique_ptr<media::RenderableGpuMemoryBufferVideoFramePool::Context>
  CreateContext(base::OnceClosure on_context_lost) override;

 private:
  const raw_ptr<GpuServiceImpl> gpu_service_;
  const raw_ptr<InProcessGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  const raw_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GMB_VIDEO_FRAME_POOL_CONTEXT_PROVIDER_IMPL_H_
