// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_VIZ_PROCESS_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_VIZ_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/observer_list.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "ui/gfx/native_widget_types.h"

class GrContext;

namespace gpu {
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
class GpuChannelManagerDelegate;
class GpuMemoryBufferManager;
class ImageFactory;
class TransferBuffer;
struct SharedMemoryLimits;
}  // namespace gpu

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace viz {
class ContextLostObserver;
class RendererSettings;

// A ContextProvider used in the viz process to setup an InProcessCommandBuffer
// for the display compositor.
class VIZ_SERVICE_EXPORT VizProcessContextProvider
    : public base::RefCountedThreadSafe<VizProcessContextProvider>,
      public ContextProvider,
      public base::trace_event::MemoryDumpProvider {
 public:
  VizProcessContextProvider(
      gpu::CommandBufferTaskExecutor* task_executor,
      gpu::SurfaceHandle surface_handle,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
      const RendererSettings& renderer_settings);

  // ContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  void AddObserver(ContextLostObserver* obs) override;
  void RemoveObserver(ContextLostObserver* obs) override;

  void SetUpdateVSyncParametersCallback(UpdateVSyncParametersCallback callback);
  void SetGpuVSyncCallback(GpuVSyncCallback callback);
  void SetGpuVSyncEnabled(bool enabled);
  bool UseRGB565PixelFormat() const;

  // Provides the GL internal format that should be used when calling
  // glCopyTexImage2D() on the default framebuffer.
  uint32_t GetCopyTextureInternalFormat();

  base::ScopedClosureRunner GetCacheBackBufferCb();

 private:
  friend class base::RefCountedThreadSafe<VizProcessContextProvider>;
  ~VizProcessContextProvider() override;

  void InitializeContext(
      gpu::CommandBufferTaskExecutor* task_executor,
      gpu::SurfaceHandle surface_handle,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
      const gpu::SharedMemoryLimits& mem_limits);
  void OnContextLost();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  const gpu::ContextCreationAttribs attributes_;

  std::unique_ptr<gpu::InProcessCommandBuffer> command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_implementation_;
  std::unique_ptr<ContextCacheController> cache_controller_;
  gpu::ContextResult context_result_;

  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;

  base::ObserverList<ContextLostObserver>::Unchecked observers_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_VIZ_PROCESS_CONTEXT_PROVIDER_H_
