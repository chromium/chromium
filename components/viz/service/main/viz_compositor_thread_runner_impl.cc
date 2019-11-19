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
#include "ui/gfx/switches.h"

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/viz/dom_agent_viz.h"
#include "components/ui_devtools/viz/overlay_agent_viz.h"
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
  auto thread = std::make_unique<base::Thread>(kThreadName);

  base::Thread::Options thread_options;

#if defined(OS_FUCHSIA)
  // An IO message pump is needed to use FIDL.
  thread_options.message_pump_type = base::MessagePumpType::IO;
#endif

#if defined(OS_MACOSX)
  // Increase the thread priority to get more reliable values in performance
  // test of macOS.
  thread_options.priority =
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseHighGPUThreadPriorityForPerfTests))
          ? base::ThreadPriority::REALTIME_AUDIO
          : thread_priority;
#else
  thread_options.priority = thread_priority;
#endif  // !defined(OS_MACOSX)

  CHECK(thread->StartWithOptions(thread_options));
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

base::SingleThreadTaskRunner* VizCompositorThreadRunnerImpl::task_runner() {
  return task_runner_.get();
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VizCompositorThreadRunnerImpl::
                                    CreateFrameSinkManagerOnCompositorThread,
                                base::Unretained(this), std::move(params),
                                nullptr, nullptr));
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params,
    gpu::CommandBufferTaskExecutor* task_executor,
    GpuServiceImpl* gpu_service) {
  // All of the unretained objects are owned on the GPU thread and destroyed
  // after VizCompositorThread has been shutdown.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VizCompositorThreadRunnerImpl::
                                    CreateFrameSinkManagerOnCompositorThread,
                                base::Unretained(this), std::move(params),
                                base::Unretained(task_executor),
                                base::Unretained(gpu_service)));
}

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
void VizCompositorThreadRunnerImpl::CreateVizDevTools(
    mojom::VizDevToolsParamsPtr params) {
  // It is safe to use Unretained(this) because |this| owns the |task_runner_|,
  // and will outlive it.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunnerImpl::CreateVizDevToolsOnCompositorThread,
          base::Unretained(this), std::move(params)));
}
#endif

void VizCompositorThreadRunnerImpl::CleanupForShutdown(
    base::OnceClosure cleanup_finished_callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunnerImpl::CleanupForShutdownOnCompositorThread,
          base::Unretained(this)),
      std::move(cleanup_finished_callback));
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManagerOnCompositorThread(
    mojom::FrameSinkManagerParamsPtr params,
    gpu::CommandBufferTaskExecutor* task_executor,
    GpuServiceImpl* gpu_service) {
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
  init_params.activation_deadline_in_frames = base::nullopt;
  if (params->use_activation_deadline) {
    init_params.activation_deadline_in_frames =
        params->activation_deadline_in_frames;
  }
  init_params.output_surface_provider = output_surface_provider_.get();
  init_params.restart_id = params->restart_id;
  init_params.run_all_compositor_stages_before_draw =
      run_all_compositor_stages_before_draw;

  frame_sink_manager_ = std::make_unique<FrameSinkManagerImpl>(init_params);
  frame_sink_manager_->BindAndSetClient(
      std::move(params->frame_sink_manager), nullptr,
      std::move(params->frame_sink_manager_client));

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  if (pending_viz_dev_tools_params_)
    InitVizDevToolsOnCompositorThread(std::move(pending_viz_dev_tools_params_));
#endif
}

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
void VizCompositorThreadRunnerImpl::CreateVizDevToolsOnCompositorThread(
    mojom::VizDevToolsParamsPtr params) {
  if (!frame_sink_manager_) {
    DCHECK(!pending_viz_dev_tools_params_);
    pending_viz_dev_tools_params_ = std::move(params);
    return;
  }
  InitVizDevToolsOnCompositorThread(std::move(params));
}

void VizCompositorThreadRunnerImpl::InitVizDevToolsOnCompositorThread(
    mojom::VizDevToolsParamsPtr params) {
  DCHECK(frame_sink_manager_);
  devtools_server_ = ui_devtools::UiDevToolsServer::CreateForViz(
      std::move(params->server_socket), params->server_port);
  auto dom_agent =
      std::make_unique<ui_devtools::DOMAgentViz>(frame_sink_manager_.get());
  auto css_agent = std::make_unique<ui_devtools::CSSAgent>(dom_agent.get());
  auto overlay_agent =
      std::make_unique<ui_devtools::OverlayAgentViz>(dom_agent.get());
  auto devtools_client = std::make_unique<ui_devtools::UiDevToolsClient>(
      "VizDevToolsClient", devtools_server_.get());
  devtools_client->AddAgent(std::move(dom_agent));
  devtools_client->AddAgent(std::move(css_agent));
  devtools_client->AddAgent(std::move(overlay_agent));
  devtools_server_->AttachClient(std::move(devtools_client));
}
#endif

void VizCompositorThreadRunnerImpl::CleanupForShutdownOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (frame_sink_manager_)
    frame_sink_manager_->ForceShutdown();
}

void VizCompositorThreadRunnerImpl::TearDownOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (server_shared_bitmap_manager_) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        server_shared_bitmap_manager_.get());
  }

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  devtools_server_.reset();
#endif
  frame_sink_manager_.reset();
  output_surface_provider_.reset();
  server_shared_bitmap_manager_.reset();
}

}  // namespace viz
