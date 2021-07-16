// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_compositor_thread_runner_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"
#include "components/viz/service/display_embedder/output_surface_provider_impl.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/scheduler_sequence.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "ui/gfx/switches.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_ui_thread.h"
#endif

namespace viz {
namespace {

const char kThreadName[] = "VizCompositorThread";

std::unique_ptr<VizCompositorThreadType> CreateAndStartCompositorThread() {
  const base::ThreadPriority thread_priority =
      base::FeatureList::IsEnabled(features::kGpuUseDisplayThreadPriority)
          ? base::ThreadPriority::DISPLAY
          : base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID)
  auto thread = std::make_unique<base::android::JavaHandlerThread>(
      kThreadName, thread_priority);
  thread->Start();
  return thread;
#else  // !defined(OS_ANDROID)

  std::unique_ptr<base::Thread> thread;
  base::Thread::Options thread_options;
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    auto* platform = ui::OzonePlatform::GetInstance();
    thread_options.message_pump_type =
        platform->GetPlatformProperties().message_pump_type_for_viz_compositor;
    thread = std::make_unique<base::Thread>(kThreadName);
  }
#endif
#if defined(USE_X11)
  if (!thread) {
    thread = std::make_unique<ui::X11UiThread>(kThreadName);
    thread_options.message_pump_type = base::MessagePumpType::UI;
  }
#endif
  if (!thread)
    thread = std::make_unique<base::Thread>(kThreadName);

#if defined(OS_FUCHSIA)
  // An IO message pump is needed to use FIDL.
  thread_options.message_pump_type = base::MessagePumpType::IO;
#endif

#if defined(OS_APPLE)
  // Increase the thread priority to get more reliable values in performance
  // test of macOS.
  thread_options.priority =
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseHighGPUThreadPriorityForPerfTests))
          ? base::ThreadPriority::REALTIME_AUDIO
          : thread_priority;
#else
  thread_options.priority = thread_priority;
#endif  // !defined(OS_APPLE)

  CHECK(thread->StartWithOptions(std::move(thread_options)));

  // Setup tracing sampler profiler as early as possible.
  thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&tracing::TracingSamplerProfiler::CreateOnChildThread));

  return thread;
#endif  // !defined(OS_ANDROID)
}

}  // namespace

VizCompositorThreadRunnerImpl::VizCompositorThreadRunnerImpl()
    : thread_(CreateAndStartCompositorThread()),
      task_runner_(thread_->task_runner()) {}

VizCompositorThreadRunnerImpl::~VizCompositorThreadRunnerImpl() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizCompositorThreadRunnerImpl::TearDownOnCompositorThread,
                     base::Unretained(this)));
  thread_->Stop();
}

base::PlatformThreadId VizCompositorThreadRunnerImpl::thread_id() {
  return thread_->GetThreadId();
}

base::SingleThreadTaskRunner* VizCompositorThreadRunnerImpl::task_runner() {
  return task_runner_.get();
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VizCompositorThreadRunnerImpl::
                                    CreateFrameSinkManagerOnCompositorThread,
                                base::Unretained(this), std::move(params),
                                nullptr, nullptr, nullptr));
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params,
    gpu::CommandBufferTaskExecutor* task_executor,
    GpuServiceImpl* gpu_service,
    gfx::RenderingPipeline* gpu_pipeline) {
  // All of the unretained objects are owned on the GPU thread and destroyed
  // after VizCompositorThread has been shutdown.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VizCompositorThreadRunnerImpl::
                                    CreateFrameSinkManagerOnCompositorThread,
                                base::Unretained(this), std::move(params),
                                base::Unretained(task_executor),
                                base::Unretained(gpu_service),
                                base::Unretained(gpu_pipeline)));
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManagerOnCompositorThread(
    mojom::FrameSinkManagerParamsPtr params,
    gpu::CommandBufferTaskExecutor* task_executor,
    GpuServiceImpl* gpu_service,
    gfx::RenderingPipeline* gpu_pipeline) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!frame_sink_manager_);
  if (features::IsUsingSkiaRenderer())
    gpu::SchedulerSequence::DefaultDisallowScheduleTaskOnCurrentThread();

  server_shared_bitmap_manager_ = std::make_unique<ServerSharedBitmapManager>();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      server_shared_bitmap_manager_.get(), "ServerSharedBitmapManager",
      task_runner_);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool headless = command_line->HasSwitch(switches::kHeadless);
  const bool run_all_compositor_stages_before_draw =
      command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw);

  if (task_executor) {
    DCHECK(gpu_service);
    // Create OutputSurfaceProvider usable for GPU + software compositing.
    auto gpu_memory_buffer_manager =
        std::make_unique<InProcessGpuMemoryBufferManager>(
            gpu_service->gpu_memory_buffer_factory(),
            gpu_service->sync_point_manager());
    auto* image_factory = gpu_service->gpu_image_factory();
    output_surface_provider_ = std::make_unique<OutputSurfaceProviderImpl>(
        gpu_service, task_executor, gpu_service,
        std::move(gpu_memory_buffer_manager), image_factory, headless);
  } else {
    // Create OutputSurfaceProvider usable for software compositing only.
    output_surface_provider_ =
        std::make_unique<OutputSurfaceProviderImpl>(headless);
  }

  // Create FrameSinkManagerImpl.
  FrameSinkManagerImpl::InitParams init_params;
  init_params.shared_bitmap_manager = server_shared_bitmap_manager_.get();
  // Set default activation deadline to infinite if client doesn't provide one.
  init_params.activation_deadline_in_frames = absl::nullopt;
  if (params->use_activation_deadline) {
    init_params.activation_deadline_in_frames =
        params->activation_deadline_in_frames;
  }
  init_params.output_surface_provider = output_surface_provider_.get();
  init_params.restart_id = params->restart_id;
  init_params.run_all_compositor_stages_before_draw =
      run_all_compositor_stages_before_draw;
  init_params.log_capture_pipeline_in_webrtc =
      features::ShouldWebRtcLogCapturePipeline();
  init_params.debug_renderer_settings = params->debug_renderer_settings;
  init_params.gpu_pipeline = gpu_pipeline;

  frame_sink_manager_ = std::make_unique<FrameSinkManagerImpl>(init_params);
  frame_sink_manager_->BindAndSetClient(
      std::move(params->frame_sink_manager), nullptr,
      std::move(params->frame_sink_manager_client));
}

void VizCompositorThreadRunnerImpl::TearDownOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (server_shared_bitmap_manager_) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        server_shared_bitmap_manager_.get());
  }

  frame_sink_manager_.reset();
  output_surface_provider_.reset();
  server_shared_bitmap_manager_.reset();
}

}  // namespace viz
