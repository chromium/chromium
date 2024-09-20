// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_in_process_context_provider.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_util.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/config/skia_limits.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/ipc/raster_in_process_context.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace viz {

TestInProcessContextProvider::TestInProcessContextProvider(
    TestContextType type,
    bool support_locking,
    gpu::raster::GrShaderCache* gr_shader_cache,
    gpu::GpuProcessShmCount* use_shader_cache_shm_count)
    : type_(type), use_shader_cache_shm_count_(use_shader_cache_shm_count) {
  CHECK(main_thread_checker_.CalledOnValidThread());
  context_thread_checker_.DetachFromThread();

  if (support_locking) {
    context_lock_.emplace();
  }
}

TestInProcessContextProvider::~TestInProcessContextProvider() {
  CHECK(main_thread_checker_.CalledOnValidThread() ||
        context_thread_checker_.CalledOnValidThread());
}

void TestInProcessContextProvider::AddRef() const {
  base::RefCountedThreadSafe<TestInProcessContextProvider>::AddRef();
}

void TestInProcessContextProvider::Release() const {
  base::RefCountedThreadSafe<TestInProcessContextProvider>::Release();
}

gpu::ContextResult TestInProcessContextProvider::BindToCurrentSequence() {
  CHECK(context_thread_checker_.CalledOnValidThread());

  if (is_bound_) {
    return gpu::ContextResult::kSuccess;
  }

  auto* holder = TestGpuServiceHolder::GetInstance();

  gpu::ContextCreationAttribs attribs;
  attribs.bind_generates_resource = false;

  if (type_ == TestContextType::kGLES2) {
    attribs.enable_gles2_interface = true;
    attribs.enable_raster_interface = false;
    attribs.enable_oop_rasterization = false;

    gles2_context_ = std::make_unique<gpu::GLInProcessContext>();
    auto result = gles2_context_->Initialize(
        TestGpuServiceHolder::GetInstance()->task_executor(), attribs,
        gpu::SharedMemoryLimits());
    CHECK_EQ(result, gpu::ContextResult::kSuccess);

    caps_ = gles2_context_->GetCapabilities();
  } else {
    bool is_gpu_raster = type_ == TestContextType::kGpuRaster;

    attribs.enable_gles2_interface = false;
    attribs.enable_raster_interface = true;
    attribs.enable_oop_rasterization = is_gpu_raster;

    raster_context_ = std::make_unique<gpu::RasterInProcessContext>();
    auto result = raster_context_->Initialize(
        holder->task_executor(), attribs, gpu::SharedMemoryLimits(),
        holder->gpu_service()->gr_shader_cache(), use_shader_cache_shm_count_);
    CHECK_EQ(result, gpu::ContextResult::kSuccess);

    caps_ = raster_context_->GetCapabilities();
    CHECK_EQ(caps_.gpu_rasterization, is_gpu_raster);
  }

  cache_controller_ = std::make_unique<ContextCacheController>(
      ContextSupport(), base::SingleThreadTaskRunner::GetCurrentDefault());
  cache_controller_->SetLock(GetLock());

  is_bound_ = true;
  return gpu::ContextResult::kSuccess;
}

gpu::gles2::GLES2Interface* TestInProcessContextProvider::ContextGL() {
  CheckValidThreadOrLockAcquired();
  CHECK(gles2_context_);
  return gles2_context_->GetImplementation();
}

gpu::raster::RasterInterface* TestInProcessContextProvider::RasterInterface() {
  CheckValidThreadOrLockAcquired();
  CHECK(raster_context_);
  return raster_context_->GetImplementation();
}

gpu::ContextSupport* TestInProcessContextProvider::ContextSupport() {
  return gles2_context_ ? gles2_context_->GetImplementation()
                        : raster_context_->GetContextSupport();
}

class GrDirectContext* TestInProcessContextProvider::GrContext() {
  CheckValidThreadOrLockAcquired();
  if (gr_context_) {
    return gr_context_->get();
  }

  if (!gles2_context_) {
    return nullptr;
  }

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  gpu::DefaultGrCacheLimitsForTests(&max_resource_cache_bytes,
                                    &max_glyph_cache_texture_bytes);
  gr_context_ = std::make_unique<skia_bindings::GrContextForGLES2Interface>(
      ContextGL(), ContextSupport(), ContextCapabilities(),
      max_resource_cache_bytes, max_glyph_cache_texture_bytes);
  cache_controller_->SetGrContext(gr_context_->get());
  return gr_context_->get();
}

gpu::SharedImageInterface*
TestInProcessContextProvider::SharedImageInterface() {
  return gles2_context_ ? gles2_context_->GetSharedImageInterface()
                        : raster_context_->GetSharedImageInterface();
}

ContextCacheController* TestInProcessContextProvider::CacheController() {
  CheckValidThreadOrLockAcquired();
  return cache_controller_.get();
}

base::Lock* TestInProcessContextProvider::GetLock() {
  return base::OptionalToPtr(context_lock_);
}

const gpu::Capabilities& TestInProcessContextProvider::ContextCapabilities()
    const {
  CheckValidThreadOrLockAcquired();
  return caps_;
}

const gpu::GpuFeatureInfo& TestInProcessContextProvider::GetGpuFeatureInfo()
    const {
  CheckValidThreadOrLockAcquired();
  return gles2_context_ ? gles2_context_->GetGpuFeatureInfo()
                        : raster_context_->GetGpuFeatureInfo();
}

void TestInProcessContextProvider::AddObserver(ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void TestInProcessContextProvider::RemoveObserver(ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

void TestInProcessContextProvider::SendOnContextLost() {
  for (auto& observer : observers_) {
    observer.OnContextLost();
  }
}

void TestInProcessContextProvider::ExecuteOnGpuThread(base::OnceClosure task) {
  CHECK(raster_context_);
  raster_context_->GetCommandBufferForTest()
      ->service_for_testing()
      ->ScheduleOutOfOrderTask(std::move(task));
}

void TestInProcessContextProvider::CheckValidThreadOrLockAcquired() const {
#if DCHECK_IS_ON()
  if (context_lock_) {
    context_lock_->AssertAcquired();
  } else {
    DCHECK(context_thread_checker_.CalledOnValidThread());
  }
#endif
}

unsigned int TestInProcessContextProvider::GetGrGLTextureFormat(
    SharedImageFormat format) const {
  return SharedImageFormatRestrictedSinglePlaneUtils::ToGLTextureStorageFormat(
      format, ContextCapabilities().angle_rgbx_internal_format);
}

GpuServiceImpl* TestInProcessContextProvider::GpuService() {
  return TestGpuServiceHolder::GetInstance()->gpu_service();
}

}  // namespace viz
