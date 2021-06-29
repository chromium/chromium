// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/compositor_gpu_thread.h"

#include <utility>

#include "base/bind_post_task.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace viz {

// static
std::unique_ptr<CompositorGpuThread> CompositorGpuThread::Create(
    gpu::GpuChannelManager* gpu_channel_manager) {
  if (!features::IsDrDcEnabled())
    return nullptr;

  auto compositor_gpu_thread =
      base::WrapUnique(new CompositorGpuThread(gpu_channel_manager));

  if (!compositor_gpu_thread->Initialize())
    return nullptr;
  return compositor_gpu_thread;
}

CompositorGpuThread::CompositorGpuThread(
    gpu::GpuChannelManager* gpu_channel_manager)
    : base::Thread("CompositorGpuThread"),
      gpu_channel_manager_(gpu_channel_manager),
      weak_ptr_factory_(this) {}

CompositorGpuThread::~CompositorGpuThread() {
  base::WaitableEvent event;
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&CompositorGpuThread::DestroyOnThread,
                                         base::Unretained(this), &event));
  event.Wait();
  base::Thread::Stop();
}

bool CompositorGpuThread::Initialize() {
  // Setup thread options.
  base::Thread::Options thread_options(base::MessagePumpType::DEFAULT, 0);
  if (base::FeatureList::IsEnabled(features::kGpuUseDisplayThreadPriority))
    thread_options.priority = base::ThreadPriority::DISPLAY;
  StartWithOptions(std::move(thread_options));

  // Initialize on display compositor gpu thread and wait for init to happen.
  base::WaitableEvent event;
  bool success = false;
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CompositorGpuThread::InitializeOnThread,
                                base::Unretained(this), &event, &success));
  event.Wait();

  return success;
}

void CompositorGpuThread::InitializeOnThread(base::WaitableEvent* event,
                                             bool* success) {
  // Create a new share group. Note that this share group is different from the
  // share group which gpu main thread uses.
  auto share_group = base::MakeRefCounted<gl::GLShareGroup>();
  auto surface = gl::init::CreateOffscreenGLSurface(gfx::Size());

  const auto& gpu_preferences = gpu_channel_manager_->gpu_preferences();
  const bool use_passthrough_decoder =
      gpu::gles2::PassthroughCommandDecoderSupported() &&
      gpu_preferences.use_passthrough_cmd_decoder;
  gl::GLContextAttribs attribs = gpu::gles2::GenerateGLContextAttribs(
      gpu::ContextCreationAttribs(), use_passthrough_decoder);

  // Create a new gl context. Note that this gl context is not part of same
  // share group which gpu main thread uses. Hence this context does not share
  // GL resources with the contexts created on gpu main thread.
  auto context =
      gl::init::CreateGLContext(share_group.get(), surface.get(), attribs);
  if (!context)
    return;

  const auto& gpu_feature_info = gpu_channel_manager_->gpu_feature_info();
  gpu_feature_info.ApplyToGLContext(context.get());

  if (!context->MakeCurrent(surface.get()))
    return;

  // Create a SharedContextState.
  shared_context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
      std::move(share_group), std::move(surface), std::move(context),
      /*use_virtualized_gl_contexts=*/false,
      gpu_channel_manager_->GetContextLostCallback(),
      gpu_preferences.gr_context_type,
      /*vulkan_context_provider=*/nullptr, /*metal_context_provider=*/nullptr,
      /*dawn_context_provider=*/nullptr,
      /*peak_memory_monitor=*/weak_ptr_factory_.GetWeakPtr());

  const auto& workarounds = gpu_channel_manager_->gpu_driver_bug_workarounds();
  auto gles2_feature_info = base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      workarounds, gpu_feature_info);

  // Initialize GL.
  if (!shared_context_state_->InitializeGL(gpu_preferences,
                                           std::move(gles2_feature_info))) {
    return;
  }

  // Initialize GrContext.
  if (!shared_context_state_->InitializeGrContext(
          gpu_preferences, workarounds, gpu_channel_manager_->gr_shader_cache(),
          /*activity_flags=*/nullptr, /*progress_reporter=*/nullptr)) {
    return;
  }
  *success = true;
  event->Signal();
}

void CompositorGpuThread::DestroyOnThread(base::WaitableEvent* event) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (shared_context_state_) {
    shared_context_state_->MakeCurrent(nullptr);
    shared_context_state_ = nullptr;
  }
  event->Signal();
}

void CompositorGpuThread::OnMemoryAllocatedChange(
    gpu::CommandBufferId id,
    uint64_t old_size,
    uint64_t new_size,
    gpu::GpuPeakMemoryAllocationSource source) {
  gpu_channel_manager_->GetOnMemoryAllocatedChangeCallback().Run(
      id, old_size, new_size, source);
}

}  // namespace viz
