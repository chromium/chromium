// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_compositor_thread_runner_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
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
#include "components/viz/service/frame_sinks/gmb_video_frame_pool_context_provider_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/service/performance_hint/hint_session.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {
namespace {

const char kThreadName[] = "VizCompositorThread";

std::unique_ptr<VizCompositorThreadType> CreateAndStartCompositorThread() {
  const base::ThreadType thread_type = base::ThreadType::kCompositing;
#if BUILDFLAG(IS_ANDROID)
  auto thread = std::make_unique<base::android::JavaHandlerThread>(kThreadName,
                                                                   thread_type);
  thread->Start();
  return thread;
#else  // !BUILDFLAG(IS_ANDROID)

  std::unique_ptr<base::Thread> thread;
  base::Thread::Options thread_options;
#if BUILDFLAG(IS_OZONE)
  auto* platform = ui::OzonePlatform::GetInstance();
  thread_options.message_pump_type =
      platform->GetPlatformProperties().message_pump_type_for_viz_compositor;
  thread = std::make_unique<base::Thread>(kThreadName);
#endif
  if (!thread)
    thread = std::make_unique<base::Thread>(kThreadName);

#if BUILDFLAG(IS_FUCHSIA)
  // An IO message pump is needed to use FIDL.
  thread_options.message_pump_type = base::MessagePumpType::IO;
#endif

#if BUILDFLAG(IS_APPLE)
  // Increase the thread priority to get more reliable values in performance
  // test of macOS.
  thread_options.thread_type =
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseHighGPUThreadPriorityForPerfTests))
          ? base::ThreadType::kRealtimeAudio
          : thread_type;
#else
  thread_options.thread_type = thread_type;
#endif  // !BUILDFLAG(IS_APPLE)

  CHECK(thread->StartWithOptions(std::move(thread_options)));

  return thread;
#endif  // !BUILDFLAG(IS_ANDROID)
}
}  // namespace

VizCompositorThreadRunnerImpl::VizCompositorThreadRunnerImpl()
    : thread_(CreateAndStartCompositorThread()),
      task_runner_(thread_->task_runner()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

VizCompositorThreadRunnerImpl::~VizCompositorThreadRunnerImpl() {
  if (shared_context_state_) {
    shared_context_state_->RemoveContextLostObserver(this);
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizCompositorThreadRunnerImpl::TearDownOnCompositorThread,
                     base::Unretained(this)));
  thread_->Stop();
}

bool VizCompositorThreadRunnerImpl::CreateHintSessionFactory(
    base::flat_set<base::PlatformThreadId> thread_ids,
    base::RepeatingClosure* wake_up_closure) {
  base::WaitableEvent event;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VizCompositorThreadRunnerImpl::
                                    CreateHintSessionFactoryOnCompositorThread,
                                base::Unretained(this), std::move(thread_ids),
                                wake_up_closure, &event));
  event.Wait();
  return !!*wake_up_closure;
}

void VizCompositorThreadRunnerImpl::CreateHintSessionFactoryOnCompositorThread(
    base::flat_set<base::PlatformThreadId> thread_ids,
    base::RepeatingClosure* wake_up_closure,
    base::WaitableEvent* event) {
  thread_ids.insert(base::PlatformThread::CurrentId());
  auto hint_session_factory = HintSessionFactory::Create(std::move(thread_ids));
  // Written this way so finch only considers the experiment active on device
  // which supports hint session.
  if (hint_session_factory) {
    hint_session_factory_ = std::move(hint_session_factory);
    *wake_up_closure = base::BindPostTask(
        task_runner_,
        base::BindRepeating(
            &VizCompositorThreadRunnerImpl::WakeUpOnCompositorThread,
            weak_factory_.GetWeakPtr()));
  }
  event->Signal();
}

void VizCompositorThreadRunnerImpl::WakeUpOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (hint_session_factory_)
    hint_session_factory_->WakeUp();
}

base::SingleThreadTaskRunner* VizCompositorThreadRunnerImpl::task_runner() {
  return task_runner_.get();
}

void VizCompositorThreadRunnerImpl::OnContextLost() {
  DCHECK(shared_context_state_);
  shared_context_state_->RemoveContextLostObserver(this);

  CreateSharedImageInterfaceOnGpu();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizCompositorThreadRunnerImpl::
                         SetFrameSinkImplSharedImageInterfaceOnCompositor,
                     base::Unretained(this),
                     base::Unretained(shared_image_interface_.get())));
}

void VizCompositorThreadRunnerImpl::
    SetFrameSinkImplSharedImageInterfaceOnCompositor(
        gpu::SharedImageInterface* shared_image_interface) {
  if (frame_sink_manager_) {
    frame_sink_manager_->SetSharedImageInterface(shared_image_interface);
  }
}

void VizCompositorThreadRunnerImpl::CreateSharedImageInterfaceOnGpu() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  shared_context_state_ =
      gl::GetGLImplementation() != gl::kGLImplementationDisabled && gpu_service_
          ? gpu_service_->GetContextState()
          : nullptr;
  if (shared_context_state_) {
    // TODO(vmpstr): This can use compositor_gpu_task_runner instead. However,
    // we also then need to create a SharedContextState from the same runner.
    // That checks that the access is happening from the thread that owns the
    // runner, which would not be the case here. All of this, however, is an
    // optimization and for now we can use main runner for these textures (also
    // since `is_for_display_compositor` is set to false to work on WebView).
    shared_image_interface_sequence_ = std::make_unique<gpu::SchedulerSequence>(
        gpu_service_->GetGpuScheduler(), gpu_service_->main_runner(),
        /*target_thread_is_always_available=*/true);
    shared_image_interface_ =
        base::MakeRefCounted<gpu::SharedImageInterfaceInProcess>(
            shared_image_interface_sequence_.get(),
            gpu_service_->sync_point_manager(), gpu_service_->gpu_preferences(),
            gpu_service_->gpu_driver_bug_workarounds(),
            gpu_service_->gpu_feature_info(), shared_context_state_.get(),
            gpu_service_->shared_image_manager(),
            /*is_for_diplay_compositor=*/false,
            gpu::SharedImageInterfaceInProcess::OwnerThread::kGpu);

    shared_context_state_->AddContextLostObserver(this);
  }
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params,
    GpuServiceImpl* gpu_service) {
  gpu_service_ = gpu_service;
  CreateSharedImageInterfaceOnGpu();
  // All of the unretained objects are owned on the GPU thread and destroyed
  // after VizCompositorThread has been shutdown.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizCompositorThreadRunnerImpl::
                         CreateFrameSinkManagerOnCompositorThread,
                     base::Unretained(this), std::move(params),
                     base::Unretained(shared_image_interface_.get())));
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManagerOnCompositorThread(
    mojom::FrameSinkManagerParamsPtr params,
    gpu::SharedImageInterface* shared_image_interface) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!frame_sink_manager_);
  gpu::SchedulerSequence::DefaultDisallowScheduleTaskOnCurrentThread();

  server_shared_bitmap_manager_ = std::make_unique<ServerSharedBitmapManager>();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      server_shared_bitmap_manager_.get(), "ServerSharedBitmapManager",
      task_runner_);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool headless = command_line->HasSwitch(switches::kHeadless);
  const bool run_all_compositor_stages_before_draw =
      command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw);

  if (gpu_service_) {
    // Create OutputSurfaceProvider usable for GPU + software compositing.
    gpu_memory_buffer_manager_ =
        std::make_unique<InProcessGpuMemoryBufferManager>(
            gpu_service_->gpu_memory_buffer_factory(),
            gpu_service_->sync_point_manager());
    output_surface_provider_ =
        std::make_unique<OutputSurfaceProviderImpl>(gpu_service_, headless);

    // Create video frame pool context provider that will enable the frame sink
    // manager to create GMB-backed video frames.
    // TODO(vmpstr): We can probably use the shared_image_interface here
    // instead of creating one for gmb video purposes.
    gmb_video_frame_pool_context_provider_ =
        std::make_unique<GmbVideoFramePoolContextProviderImpl>(
            gpu_service_, gpu_memory_buffer_manager_.get());
  } else {
    // Create OutputSurfaceProvider usable for software compositing only.
    output_surface_provider_ =
        std::make_unique<OutputSurfaceProviderImpl>(headless);
  }

  // Create FrameSinkManagerImpl.
  FrameSinkManagerImpl::InitParams init_params;
  init_params.shared_bitmap_manager = server_shared_bitmap_manager_.get();
  // Set default activation deadline to infinite if client doesn't provide one.
  init_params.activation_deadline_in_frames = std::nullopt;
  if (params->use_activation_deadline) {
    init_params.activation_deadline_in_frames =
        params->activation_deadline_in_frames;
  }
  init_params.output_surface_provider = output_surface_provider_.get();
  init_params.gmb_context_provider =
      gmb_video_frame_pool_context_provider_.get();
  init_params.restart_id = params->restart_id;
  init_params.run_all_compositor_stages_before_draw =
      run_all_compositor_stages_before_draw;
  init_params.log_capture_pipeline_in_webrtc =
      features::ShouldWebRtcLogCapturePipeline();
  init_params.debug_renderer_settings = params->debug_renderer_settings;
  if (gpu_service_) {
    init_params.host_process_id = gpu_service_->host_process_id();
  }
  init_params.hint_session_factory = hint_session_factory_.get();
  init_params.shared_image_interface = shared_image_interface;

  frame_sink_manager_ = std::make_unique<FrameSinkManagerImpl>(init_params);
  frame_sink_manager_->BindAndSetClient(
      std::move(params->frame_sink_manager), nullptr,
      std::move(params->frame_sink_manager_client));
}

void VizCompositorThreadRunnerImpl::TearDownOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  weak_factory_.InvalidateWeakPtrs();

  if (server_shared_bitmap_manager_) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        server_shared_bitmap_manager_.get());
  }

  frame_sink_manager_.reset();
  hint_session_factory_.reset();
  output_surface_provider_.reset();
  gmb_video_frame_pool_context_provider_.reset();
  gpu_memory_buffer_manager_.reset();
  server_shared_bitmap_manager_.reset();
}

}  // namespace viz
