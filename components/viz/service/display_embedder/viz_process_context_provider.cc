// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/viz_process_context_provider.h"

#include <stdint.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/common/resources/platform_color.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/skia_bindings/gles2_implementation_with_grcontext_support.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace viz {

namespace {

gpu::ContextCreationAttribs CreateAttributes(bool requires_alpha_channel) {
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = requires_alpha_channel ? 8 : -1;
  attributes.depth_size = 0;
#if defined(OS_CHROMEOS)
  // Chrome OS uses surfaceless when running on a real device and stencil
  // buffers can then be added dynamically so supporting them does not have an
  // impact on normal usage. If we are not running on a real Chrome OS device
  // but instead on a workstation for development, then stencil support is
  // useful as it allows the overdraw feedback debugging feature to be used.
  attributes.stencil_size = 8;
#else
  attributes.stencil_size = 0;
#endif
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.fail_if_major_perf_caveat = false;
  attributes.lose_context_when_out_of_memory = true;

#if defined(OS_ANDROID)
  // TODO(cblume): We should add wide gamut code here, setting
  // attributes.color_space.

  if (requires_alpha_channel) {
    attributes.alpha_size = 8;
  } else if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512) {
    // See compositor_impl_android.cc for more information about this.
    // It is inside GetCompositorContextAttributes().
    attributes.alpha_size = 0;
    attributes.red_size = 5;
    attributes.green_size = 6;
    attributes.blue_size = 5;
  }

  attributes.enable_swap_timestamps_if_supported = true;
#endif  // defined(OS_ANDROID)

  return attributes;
}

void UmaRecordContextLost(ContextLostReason reason) {
  UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.DisplayCompositor", reason);
}

}  // namespace

VizProcessContextProvider::VizProcessContextProvider(
    scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    const gpu::SharedMemoryLimits& limits,
    bool requires_alpha_channel)
    : attributes_(CreateAttributes(requires_alpha_channel)) {
  InitializeContext(std::move(task_executor), surface_handle,
                    gpu_memory_buffer_manager, image_factory,
                    gpu_channel_manager_delegate, limits);

  if (context_result_ == gpu::ContextResult::kSuccess) {
    // |gles2_implementation_| is owned here so bind an unretained pointer or
    // there will be a circular reference preventing destruction.
    gles2_implementation_->SetLostContextCallback(base::BindOnce(
        &VizProcessContextProvider::OnContextLost, base::Unretained(this)));

    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "VizProcessContextProvider", base::ThreadTaskRunnerHandle::Get());
  } else {
    UmaRecordContextLost(CONTEXT_INIT_FAILED);
  }
}

VizProcessContextProvider::~VizProcessContextProvider() {
  if (context_result_ == gpu::ContextResult::kSuccess) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }
}

void VizProcessContextProvider::AddRef() const {
  base::RefCountedThreadSafe<VizProcessContextProvider>::AddRef();
}

void VizProcessContextProvider::Release() const {
  base::RefCountedThreadSafe<VizProcessContextProvider>::Release();
}

gpu::ContextResult VizProcessContextProvider::BindToCurrentThread() {
  return context_result_;
}

gpu::gles2::GLES2Interface* VizProcessContextProvider::ContextGL() {
  return gles2_implementation_.get();
}

gpu::ContextSupport* VizProcessContextProvider::ContextSupport() {
  return gles2_implementation_.get();
}

class GrContext* VizProcessContextProvider::GrContext() {
  if (gr_context_)
    return gr_context_->get();

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  gpu::raster::DetermineGrCacheLimitsFromAvailableMemory(
      &max_resource_cache_bytes, &max_glyph_cache_texture_bytes);

  gr_context_ = std::make_unique<skia_bindings::GrContextForGLES2Interface>(
      ContextGL(), ContextSupport(), ContextCapabilities(),
      max_resource_cache_bytes, max_glyph_cache_texture_bytes);
  return gr_context_->get();
}

gpu::SharedImageInterface* VizProcessContextProvider::SharedImageInterface() {
  return command_buffer_->GetSharedImageInterface();
}

ContextCacheController* VizProcessContextProvider::CacheController() {
  return cache_controller_.get();
}

base::Lock* VizProcessContextProvider::GetLock() {
  // Locking isn't supported on display compositor contexts.
  return nullptr;
}

const gpu::Capabilities& VizProcessContextProvider::ContextCapabilities()
    const {
  return command_buffer_->GetCapabilities();
}

const gpu::GpuFeatureInfo& VizProcessContextProvider::GetGpuFeatureInfo()
    const {
  return command_buffer_->GetGpuFeatureInfo();
}

void VizProcessContextProvider::AddObserver(ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void VizProcessContextProvider::RemoveObserver(ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

void VizProcessContextProvider::SetUpdateVSyncParametersCallback(
    const gpu::InProcessCommandBuffer::UpdateVSyncParametersCallback&
        callback) {
  command_buffer_->SetUpdateVSyncParametersCallback(callback);
}

bool VizProcessContextProvider::UseRGB565PixelFormat() const {
  return attributes_.alpha_size == 0 && attributes_.red_size == 5 &&
         attributes_.green_size == 6 && attributes_.blue_size == 5;
}

uint32_t VizProcessContextProvider::GetCopyTextureInternalFormat() {
  return attributes_.alpha_size > 0 ? GL_RGBA : GL_RGB;
}

void VizProcessContextProvider::InitializeContext(
    scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    const gpu::SharedMemoryLimits& mem_limits) {
  const bool is_offscreen = surface_handle == gpu::kNullSurfaceHandle;

  command_buffer_ =
      std::make_unique<gpu::InProcessCommandBuffer>(std::move(task_executor));
  context_result_ = command_buffer_->Initialize(
      /*surface=*/nullptr, is_offscreen, surface_handle, attributes_,
      /*share_command_buffer=*/nullptr, gpu_memory_buffer_manager,
      image_factory, gpu_channel_manager_delegate,
      base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  if (context_result_ != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize InProcessCommmandBuffer";
    return;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_ =
      std::make_unique<gpu::gles2::GLES2CmdHelper>(command_buffer_.get());
  context_result_ = gles2_helper_->Initialize(mem_limits.command_buffer_size);
  if (context_result_ != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize GLES2CmdHelper";
    return;
  }

  transfer_buffer_ = std::make_unique<gpu::TransferBuffer>(gles2_helper_.get());

  // Create the object exposing the OpenGL API.
  gles2_implementation_ =
      std::make_unique<skia_bindings::GLES2ImplementationWithGrContextSupport>(
          gles2_helper_.get(), /*share_group=*/nullptr, transfer_buffer_.get(),
          attributes_.bind_generates_resource,
          attributes_.lose_context_when_out_of_memory,
          /*support_client_side_arrays=*/false, command_buffer_.get());

  context_result_ = gles2_implementation_->Initialize(mem_limits);
  if (context_result_ != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize GLES2Implementation";
    return;
  }

  cache_controller_ = std::make_unique<ContextCacheController>(
      gles2_implementation_.get(), base::ThreadTaskRunnerHandle::Get());
}

void VizProcessContextProvider::OnContextLost() {
  for (auto& observer : observers_)
    observer.OnContextLost();
  if (gr_context_)
    gr_context_->OnLostContext();

  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  UmaRecordContextLost(
      GetContextLostReason(state.error, state.context_lost_reason));
}

bool VizProcessContextProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_EQ(context_result_, gpu::ContextResult::kSuccess);

  gles2_implementation_->OnMemoryDump(args, pmd);
  gles2_helper_->OnMemoryDump(args, pmd);

  if (gr_context_) {
    gpu::raster::DumpGrMemoryStatistics(
        gr_context_->get(), pmd,
        gles2_implementation_->ShareGroupTracingGUID());
  }
  return true;
}

}  // namespace viz
