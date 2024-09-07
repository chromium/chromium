// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/compositor_gpu_thread.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

namespace viz {

// static
std::unique_ptr<CompositorGpuThread> CompositorGpuThread::MaybeCreate(
    const CreateParams& params) {
  DCHECK(params.gpu_channel_manager);

  if (!features::IsDrDcEnabled() ||
      params.gpu_channel_manager->gpu_driver_bug_workarounds().disable_drdc) {
    return nullptr;
  }

#if DCHECK_IS_ON()
#if BUILDFLAG(IS_ANDROID)
  // When using angle via enabling passthrough command decoder on android, angle
  // context virtualization group extension should be enabled. Also since angle
  // currently always enables this extension with GL backend, we are adding
  // DCHECK() to ensure that instead of enabling/disabling DrDc based on the
  // extension.
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kOpenGLES) {
    gl::GLDisplayEGL* display_egl = params.display->GetAs<gl::GLDisplayEGL>();
    DCHECK(display_egl->ext->b_EGL_ANGLE_context_virtualization);
  }
#endif
#endif  // DCHECK_IS_ON()

  auto compositor_gpu_thread = base::WrapUnique(new CompositorGpuThread(
      params.gpu_channel_manager, params.display, params.enable_watchdog));

#if BUILDFLAG(ENABLE_VULKAN)
  // Create a VulkanContextProvider.
  if (params.vulkan_implementation && params.device_queue) {
    auto* device_queue = params.device_queue.get();
    auto compositor_thread_device_queue =
        std::make_unique<gpu::VulkanDeviceQueue>(
            device_queue->GetVulkanInstance());
    compositor_thread_device_queue->InitializeForCompositorGpuThread(
        device_queue->GetVulkanPhysicalDevice(),
        device_queue->GetVulkanDevice(), device_queue->GetVulkanQueue(),
        device_queue->GetVulkanQueueIndex(), device_queue->enabled_extensions(),
        device_queue->enabled_device_features_2(),
        device_queue->vma_allocator());
    compositor_gpu_thread->vulkan_context_provider_ =
        VulkanInProcessContextProvider::CreateForCompositorGpuThread(
            params.vulkan_implementation,
            std::move(compositor_thread_device_queue),
            params.gpu_channel_manager->gpu_preferences()
                .vulkan_sync_cpu_memory_limit);
  }
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
  if (params.gpu_channel_manager->gpu_preferences().gr_context_type ==
      gpu::GrContextType::kGraphiteDawn) {
    compositor_gpu_thread->dawn_context_provider_ =
        gpu::DawnContextProvider::CreateWithSharedDevice(
            params.dawn_context_provider);
  }
#endif

  if (!compositor_gpu_thread->Initialize()) {
    return nullptr;
  }
  return compositor_gpu_thread;
}

CompositorGpuThread::CompositorGpuThread(
    gpu::GpuChannelManager* gpu_channel_manager,
    gl::GLDisplay* display,
    bool enable_watchdog)
    : base::Thread("CompositorGpuThread"),
      gpu_channel_manager_(gpu_channel_manager),
      enable_watchdog_(enable_watchdog),
      display_(display),
      weak_ptr_factory_(this) {}

CompositorGpuThread::~CompositorGpuThread() {
  base::Thread::Stop();
}

scoped_refptr<gpu::SharedContextState>
CompositorGpuThread::GetSharedContextState() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (shared_context_state_ && !shared_context_state_->context_lost())
    return shared_context_state_;

  // Cleanup the previous context if any.
  shared_context_state_.reset();

  // Create a new share group. Note that this share group is different from the
  // share group which gpu main thread uses.
  auto share_group = base::MakeRefCounted<gl::GLShareGroup>();
  auto surface = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());

  const auto& gpu_preferences = gpu_channel_manager_->gpu_preferences();

  const bool use_passthrough_decoder =
      gpu::gles2::PassthroughCommandDecoderSupported() &&
      gpu_preferences.use_passthrough_cmd_decoder;
  gl::GLContextAttribs attribs =
      gpu::gles2::GenerateGLContextAttribsForCompositor(
          use_passthrough_decoder);
  attribs.angle_context_virtualization_group_number =
      gl::AngleContextVirtualizationGroup::kDrDc;

  attribs.can_skip_validation = !features::IsANGLEValidationEnabled();

  // Compositor thread context doesn't need access textures and semaphores
  // created with other contexts.
  attribs.global_texture_share_group = false;
  attribs.global_semaphore_share_group = false;

  // Create a new gl context. Note that this gl context is not part of same
  // share group which gpu main thread uses. Hence this context does not share
  // GL resources with the contexts created on gpu main thread.
  auto context =
      gl::init::CreateGLContext(share_group.get(), surface.get(), attribs);

  if (!context && !features::UseGles2ForOopR()) {
    LOG(ERROR) << "Failed to create GLES3 context, fallback to GLES2.";
    attribs.client_major_es_version = 2;
    attribs.client_minor_es_version = 0;
    context =
        gl::init::CreateGLContext(share_group.get(), surface.get(), attribs);
  }

  if (!context) {
    LOG(ERROR) << "Failed to create shared context";
    return nullptr;
  }

  const auto& gpu_feature_info = gpu_channel_manager_->gpu_feature_info();
  gpu_feature_info.ApplyToGLContext(context.get());

  if (!context->MakeCurrent(surface.get())) {
    LOG(ERROR) << "Failed to make context current";
    return nullptr;
  }

  const auto& workarounds = gpu_channel_manager_->gpu_driver_bug_workarounds();

  // Create a SharedContextState.
  auto shared_context_state = base::MakeRefCounted<gpu::SharedContextState>(
      std::move(share_group), std::move(surface), std::move(context),
      /*use_virtualized_gl_contexts=*/false,
      gpu_channel_manager_->GetContextLostCallback(),
      gpu_preferences.gr_context_type,
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_context_provider_.get(),
#else
      /*vulkan_context_provider=*/nullptr,
#endif
      /*metal_context_provider=*/nullptr,
#if BUILDFLAG(SKIA_USE_DAWN)
      dawn_context_provider_.get(),
#else
      /*dawn_context_provider=*/nullptr,
#endif
      /*peak_memory_monitor=*/weak_ptr_factory_.GetWeakPtr(),
      /*created_on_compositor_gpu_thread=*/true);

  auto gles2_feature_info = base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      workarounds, gpu_feature_info);

  // Initialize GL.
  if (!shared_context_state->InitializeGL(gpu_preferences,
                                          std::move(gles2_feature_info))) {
    LOG(ERROR) << "Failed to initialize GL for DrDC SharedContextState";
    return nullptr;
  }

  // Initialize Skia.
  if (!shared_context_state->InitializeSkia(
          gpu_preferences, workarounds, gpu_channel_manager_->gr_shader_cache(),
          gpu_channel_manager_->use_shader_cache_shm_count(),
          /*progress_reporter=*/nullptr)) {
    LOG(ERROR) << "Failed to Initialize Skia for DrDC SharedContextState";
  }
  shared_context_state_ = std::move(shared_context_state);
  return shared_context_state_;
}

bool CompositorGpuThread::Initialize() {
  // Setup thread options.
  base::Thread::Options thread_options(base::MessagePumpType::DEFAULT, 0);
  thread_options.thread_type = base::ThreadType::kDisplayCritical;
  StartWithOptions(std::move(thread_options));

  // Wait until thread is started and Init() is executed in order to return
  // updated |init_succeeded_|.
  WaitUntilThreadStarted();
  return init_succeeded_;
}

void CompositorGpuThread::HandleMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  // Context should be current for cache/memory cleanup.
  if (shared_context_state_ &&
      shared_context_state_->MakeCurrent(nullptr, /*needs_gl=*/true)) {
    shared_context_state_->PurgeMemory(memory_pressure_level);
  }
}

void CompositorGpuThread::Init() {
  const auto& gpu_preferences = gpu_channel_manager_->gpu_preferences();
  if (enable_watchdog_ && gpu_channel_manager_->watchdog()) {
    watchdog_thread_ = gpu::GpuWatchdogThread::Create(
        gpu_preferences.watchdog_starts_backgrounded,
        gpu_channel_manager_->watchdog(), "GpuWatchdog_Compositor");
    watchdog_thread_->OnInitComplete();
  }

  // Making sure to create the |memory_pressure_listener_| on
  // CompositorGpuThread since this callback will be called on the thread it was
  // created on.
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&CompositorGpuThread::HandleMemoryPressure,
                                     base::Unretained(this))),
  init_succeeded_ = true;
}

void CompositorGpuThread::CleanUp() {
  // Destroying |memory_pressure_listener_| here to ensure its destroyed on the
  // same thread on which it was created on.
  memory_pressure_listener_.reset();
  if (watchdog_thread_)
    watchdog_thread_->OnGpuProcessTearDown();

  weak_ptr_factory_.InvalidateWeakPtrs();
  if (shared_context_state_) {
    shared_context_state_->MakeCurrent(nullptr);
    shared_context_state_ = nullptr;
  }

  // WatchDogThread destruction should happen on the CompositorGpuThread.
  watchdog_thread_.reset();
}

void CompositorGpuThread::OnMemoryAllocatedChange(
    gpu::CommandBufferId id,
    uint64_t old_size,
    uint64_t new_size,
    gpu::GpuPeakMemoryAllocationSource source) {
  gpu_channel_manager_->GetOnMemoryAllocatedChangeCallback().Run(
      id, old_size, new_size, source);
}

void CompositorGpuThread::OnBackgrounded() {
  if (watchdog_thread_)
    watchdog_thread_->OnBackgrounded();

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CompositorGpuThread::OnBackgroundedOnCompositorGpuThread,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CompositorGpuThread::OnBackgroundedOnCompositorGpuThread() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (shared_context_state_) {
    shared_context_state_->PurgeMemory(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
  }
}

void CompositorGpuThread::OnBackgroundCleanup() {
  LoseContext();
}

void CompositorGpuThread::OnForegrounded() {
  if (watchdog_thread_)
    watchdog_thread_->OnForegrounded();
}

void CompositorGpuThread::LoseContext() {
  if (!task_runner()->BelongsToCurrentThread()) {
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(&CompositorGpuThread::LoseContext,
                                           weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (shared_context_state_) {
    shared_context_state_->MarkContextLost();
    shared_context_state_.reset();
  }
}

}  // namespace viz
