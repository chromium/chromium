// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_in_process_context_provider.h"

#include <stdint.h>
#include <utility>

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/ipc/raster_in_process_context.h"
#include "gpu/ipc/test_gpu_thread_holder.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/native_widget_types.h"

namespace viz {

namespace {

std::unique_ptr<gpu::GLInProcessContext> CreateGLInProcessContext(
    TestGpuMemoryBufferManager* gpu_memory_buffer_manager,
    TestImageFactory* image_factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool oop_raster) {
  const bool is_offscreen = true;
  gpu::ContextCreationAttribs attribs;
  attribs.alpha_size = -1;
  attribs.depth_size = 24;
  attribs.stencil_size = 8;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;
  attribs.enable_oop_rasterization = oop_raster;

  auto context = std::make_unique<gpu::GLInProcessContext>();
  auto result = context->Initialize(
      TestGpuServiceHolder::GetInstance()->task_executor(), nullptr,
      is_offscreen, gpu::kNullSurfaceHandle, attribs, gpu::SharedMemoryLimits(),
      gpu_memory_buffer_manager, image_factory, std::move(task_runner));

  DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  return context;
}

}  // namespace

std::unique_ptr<gpu::GLInProcessContext> CreateTestInProcessContext() {
  return CreateGLInProcessContext(nullptr, nullptr,
                                  base::ThreadTaskRunnerHandle::Get(), false);
}

TestInProcessContextProvider::TestInProcessContextProvider(
    bool enable_oop_rasterization,
    bool support_locking,
    gpu::raster::GrShaderCache* gr_shader_cache,
    gpu::GpuProcessActivityFlags* activity_flags)
    : enable_oop_rasterization_(enable_oop_rasterization),
      activity_flags_(activity_flags) {
  if (support_locking)
    context_lock_.emplace();
}

TestInProcessContextProvider::~TestInProcessContextProvider() = default;

void TestInProcessContextProvider::AddRef() const {
  base::RefCountedThreadSafe<TestInProcessContextProvider>::AddRef();
}

void TestInProcessContextProvider::Release() const {
  base::RefCountedThreadSafe<TestInProcessContextProvider>::Release();
}

gpu::ContextResult TestInProcessContextProvider::BindToCurrentThread() {
  if (enable_oop_rasterization_) {
    gpu::ContextCreationAttribs attribs;
    attribs.bind_generates_resource = false;
    attribs.enable_oop_rasterization = true;
    attribs.enable_raster_interface = true;
    attribs.enable_gles2_interface = false;

    raster_context_ = std::make_unique<gpu::RasterInProcessContext>();
    auto* holder = TestGpuServiceHolder::GetInstance();
    auto result = raster_context_->Initialize(
        holder->task_executor(), attribs, gpu::SharedMemoryLimits(),
        &gpu_memory_buffer_manager_, &image_factory_,
        /*gpu_channel_manager_delegate=*/nullptr,
        holder->gpu_service()->gr_shader_cache(), activity_flags_);
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);

    cache_controller_.reset(
        new ContextCacheController(raster_context_->GetContextSupport(),
                                   base::ThreadTaskRunnerHandle::Get()));
  } else {
    gles2_context_ = CreateGLInProcessContext(
        &gpu_memory_buffer_manager_, &image_factory_,
        base::ThreadTaskRunnerHandle::Get(), false /* oop_raster */);
    cache_controller_.reset(
        new ContextCacheController(gles2_context_->GetImplementation(),
                                   base::ThreadTaskRunnerHandle::Get()));
    raster_implementation_gles2_ =
        std::make_unique<gpu::raster::RasterImplementationGLES>(
            gles2_context_->GetImplementation());
  }

  cache_controller_->SetLock(GetLock());
  return gpu::ContextResult::kSuccess;
}

gpu::gles2::GLES2Interface* TestInProcessContextProvider::ContextGL() {
  return gles2_context_->GetImplementation();
}

gpu::raster::RasterInterface* TestInProcessContextProvider::RasterInterface() {
  if (raster_context_) {
    return raster_context_->GetImplementation();
  } else {
    return raster_implementation_gles2_.get();
  }
}

gpu::ContextSupport* TestInProcessContextProvider::ContextSupport() {
  if (gles2_context_) {
    return gles2_context_->GetImplementation();
  } else {
    return raster_context_->GetContextSupport();
  }
}

class GrContext* TestInProcessContextProvider::GrContext() {
  if (gr_context_)
    return gr_context_->get();

  if (!gles2_context_) {
    return nullptr;
  }

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  gpu::raster::DefaultGrCacheLimitsForTests(&max_resource_cache_bytes,
                                            &max_glyph_cache_texture_bytes);
  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(
      ContextGL(), ContextSupport(), ContextCapabilities(),
      max_resource_cache_bytes, max_glyph_cache_texture_bytes));
  cache_controller_->SetGrContext(gr_context_->get());
  return gr_context_->get();
}

gpu::SharedImageInterface*
TestInProcessContextProvider::SharedImageInterface() {
  if (gles2_context_) {
    return gles2_context_->GetSharedImageInterface();
  } else {
    return raster_context_->GetSharedImageInterface();
  }
}

ContextCacheController* TestInProcessContextProvider::CacheController() {
  return cache_controller_.get();
}

base::Lock* TestInProcessContextProvider::GetLock() {
  return base::OptionalOrNullptr(context_lock_);
}

const gpu::Capabilities& TestInProcessContextProvider::ContextCapabilities()
    const {
  if (gles2_context_) {
    return gles2_context_->GetCapabilities();
  } else {
    return raster_context_->GetCapabilities();
  }
}

const gpu::GpuFeatureInfo& TestInProcessContextProvider::GetGpuFeatureInfo()
    const {
  return gpu_feature_info_;
}

void TestInProcessContextProvider::ExecuteOnGpuThread(base::OnceClosure task) {
  DCHECK(raster_context_);
  raster_context_->GetCommandBufferForTest()
      ->service_for_testing()
      ->ScheduleOutOfOrderTask(std::move(task));
}

}  // namespace viz
