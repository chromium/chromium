// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_compositor_thread_runner_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/platform_thread_metrics.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/output_surface_provider_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/frame_sinks/gmb_video_frame_pool_context_provider_impl.h"
#include "components/viz/service/frame_sinks/shared_image_interface_provider.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/service/performance_hint/hint_session.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {
namespace {

const char kThreadName[] = "VizCompositorThread";

#if BUILDFLAG(IS_ANDROID)
class VizCompositorThread : public base::android::JavaHandlerThread {
 public:
  using ParentType = base::android::JavaHandlerThread;
  explicit VizCompositorThread(base::ThreadType thread_type)
      : ParentType(kThreadName, thread_type) {}
#else   // BUILDFLAG(IS_ANDROID)
class VizCompositorThread : public base::Thread {
 public:
  using ParentType = base::Thread;
  VizCompositorThread() : ParentType(kThreadName) {}
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  void Init() override {
    ParentType::Init();
    if (base::HangWatcher::IsCompositorThreadHangWatchingEnabled()) {
      unregister_thread_closure_ = base::HangWatcher::RegisterThread(
          base::HangWatcher::ThreadType::kCompositorThread);
    }
  }
  void CleanUp() override {
    unregister_thread_closure_.RunAndReset();
    ParentType::CleanUp();
  }

  base::ScopedClosureRunner unregister_thread_closure_;
};

std::unique_ptr<VizCompositorThreadType> CreateAndStartCompositorThread(
    base::TaskObserver* task_observer) {
  const base::ThreadType thread_type = base::ThreadType::kDisplayCritical;
#if BUILDFLAG(IS_ANDROID)
  auto thread = std::make_unique<VizCompositorThread>(thread_type);
  thread->Start();
  thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics(
            "VizCompositor");
        base::PlatformThreadPriorityMonitor::Get().RegisterCurrentThread(
            "VizCompositor");
      }));
  return thread;
#else  // !BUILDFLAG(IS_ANDROID)

  std::unique_ptr<base::Thread> thread;
  base::Thread::Options thread_options;
  // DirectReceiver requires an I/O MessagePump, or the pump to expose an
  // IOWatcher like MessagePumpAndroid.
  const bool should_use_io_pump =
      mojo::IsDirectReceiverSupported() &&
      (features::IsVizDirectCompositorThreadIpcNonRootEnabled() ||
       features::IsVizDirectCompositorThreadIpcFrameSinkManagerEnabled());
  if (should_use_io_pump) {
    thread_options.message_pump_type = base::MessagePumpType::IO;
  }
#if BUILDFLAG(IS_OZONE)
  auto* platform = ui::OzonePlatform::GetInstance();
  thread_options.message_pump_type =
      platform->GetPlatformProperties().message_pump_type_for_viz_compositor;
  thread = std::make_unique<VizCompositorThread>();
#endif
  if (!thread)
    thread = std::make_unique<VizCompositorThread>();

#if BUILDFLAG(IS_FUCHSIA)
  // An IO message pump is needed to use FIDL.
  thread_options.message_pump_type = base::MessagePumpType::IO;
#elif BUILDFLAG(IS_MAC)
  // The feature kCADisplayLink needs the thread type NS_RUNLOOP to run on the
  // current thread' runloop.
  // See [ca_display_link addToRunLoop:NSRunLoop.currentRunLoop].
  thread_options.message_pump_type = base::MessagePumpType::NS_RUNLOOP;
#endif

  thread_options.thread_type = thread_type;
  thread_options.task_observer = task_observer;

  CHECK(thread->StartWithOptions(std::move(thread_options)));

  thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics(
            "VizCompositor");
      }));

  return thread;
#endif  // !BUILDFLAG(IS_ANDROID)
}
}  // namespace

VizCompositorThreadRunnerImpl::VizCompositorThreadRunnerImpl() {
  if (base::FeatureList::IsEnabled(
          base::features::kBoostCompositorThreadsPriorityWhenIdle)) {
    scenario_priority_boost_.emplace(
        base::ThreadType::kInteractive, base::BindRepeating([]() {
          return performance_scenarios::CurrentScenariosMatch(
              performance_scenarios::ScenarioScope::kGlobal,
              performance_scenarios::kDefaultIdleScenarios);
        }));
  }
  thread_ = CreateAndStartCompositorThread(
      scenario_priority_boost_.has_value() ? &scenario_priority_boost_.value()
                                           : nullptr);
  task_runner_ = thread_->task_runner();
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

VizCompositorThreadRunnerImpl::~VizCompositorThreadRunnerImpl() {
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

void VizCompositorThreadRunnerImpl::NotifyWorkloadIncrease() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VizCompositorThreadRunnerImpl::
                                    NotifyWorkloadIncreaseOnCompositorThread,
                                base::Unretained(this)));
}

void VizCompositorThreadRunnerImpl::NotifyWorkloadIncreaseOnCompositorThread() {
  if (hint_session_factory_) {
    hint_session_factory_->NotifyWorkloadIncrease();
  }
}

void VizCompositorThreadRunnerImpl::WakeUpOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (hint_session_factory_)
    hint_session_factory_->WakeUp();
}

base::SingleThreadTaskRunner* VizCompositorThreadRunnerImpl::task_runner() {
  return task_runner_.get();
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params,
    GpuServiceImpl* gpu_service) {
  shared_image_interface_provider_ =
      std::make_unique<SharedImageInterfaceProvider>(gpu_service);

  // All of the unretained objects are owned on the GPU thread and destroyed
  // after VizCompositorThread has been shutdown.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VizCompositorThreadRunnerImpl::
                                    CreateFrameSinkManagerOnCompositorThread,
                                base::Unretained(this), std::move(params),
                                base::Unretained(gpu_service)));
}

void VizCompositorThreadRunnerImpl::CreateFrameSinkManagerOnCompositorThread(
    mojom::FrameSinkManagerParamsPtr params,
    GpuServiceImpl* gpu_service) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!frame_sink_manager_);
  gpu::SchedulerSequence::DefaultDisallowScheduleTaskOnCurrentThread();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool headless = command_line->HasSwitch(switches::kHeadless);
  const bool run_all_compositor_stages_before_draw =
      command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw);

  if (gpu_service) {
    // Create OutputSurfaceProvider usable for GPU + software compositing.
    output_surface_provider_ =
        std::make_unique<OutputSurfaceProviderImpl>(gpu_service, headless);

    // Create video frame pool context provider that will enable the frame sink
    // manager to create video frames backed by mappable SharedImages.
    gmb_video_frame_pool_context_provider_ =
        std::make_unique<GmbVideoFramePoolContextProviderImpl>(gpu_service);
  } else {
    // Create OutputSurfaceProvider usable for software compositing only.
    output_surface_provider_ =
        std::make_unique<OutputSurfaceProviderImpl>(headless);
  }

  // Create FrameSinkManagerImpl.
  FrameSinkManagerImpl::InitParams init_params;

  // Set default activation deadline to infinite if client doesn't provide one.
  init_params.activation_deadline_in_frames = std::nullopt;
  if (params->use_activation_deadline) {
    init_params.activation_deadline_in_frames =
        params->activation_deadline_in_frames;
  }
  init_params.output_surface_provider = output_surface_provider_.get();
  init_params.gpu_service = gpu_service;
  init_params.gmb_context_provider =
      gmb_video_frame_pool_context_provider_.get();
  init_params.restart_id = params->restart_id;
  init_params.run_all_compositor_stages_before_draw =
      run_all_compositor_stages_before_draw;
  init_params.log_capture_pipeline_in_webrtc =
      features::ShouldWebRtcLogCapturePipeline();
  init_params.debug_renderer_settings = params->debug_renderer_settings;
  if (gpu_service) {
    init_params.host_process_id = gpu_service->host_process_id();
  }
  init_params.hint_session_factory = hint_session_factory_.get();

  frame_sink_manager_ = std::make_unique<FrameSinkManagerImpl>(init_params);
  frame_sink_manager_->BindAndSetClient(
      std::move(params->frame_sink_manager), nullptr,
      std::move(params->frame_sink_manager_client),
      shared_image_interface_provider_.get());
}

void VizCompositorThreadRunnerImpl::RequestBeginFrameForGpuService(
    bool toggle) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizCompositorThreadRunnerImpl::
                         RequestBeginFrameForGpuServiceOnCompositorThread,
                     base::Unretained(this), toggle));
}

void VizCompositorThreadRunnerImpl::
    RequestBeginFrameForGpuServiceOnCompositorThread(bool toggle) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  frame_sink_manager_->RequestBeginFrameForGpuService(toggle);
}

void VizCompositorThreadRunnerImpl::TearDownOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  weak_factory_.InvalidateWeakPtrs();

  frame_sink_manager_.reset();
  hint_session_factory_.reset();
  output_surface_provider_.reset();
  gmb_video_frame_pool_context_provider_.reset();
}

}  // namespace viz
