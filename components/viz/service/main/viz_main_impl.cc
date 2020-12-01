// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_main_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/ui_devtools/buildflags.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "media/gpu/buildflags.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "skia/ext/legacy_display_globals.h"

namespace {

std::unique_ptr<base::Thread> CreateAndStartIOThread() {
  // TODO(sad): We do not need the IO thread once gpu has a separate process.
  // It should be possible to use |main_task_runner_| for doing IO tasks.
  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  // TODO(reveman): Remove this in favor of setting it explicitly for each
  // type of process.
  if (base::FeatureList::IsEnabled(features::kGpuUseDisplayThreadPriority))
    thread_options.priority = base::ThreadPriority::DISPLAY;
  auto io_thread = std::make_unique<base::Thread>("GpuIOThread");
  CHECK(io_thread->StartWithOptions(thread_options));
  return io_thread;
}

}  // namespace

namespace viz {

VizMainImpl::ExternalDependencies::ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::~ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::ExternalDependencies(
    ExternalDependencies&& other) = default;

VizMainImpl::ExternalDependencies& VizMainImpl::ExternalDependencies::operator=(
    ExternalDependencies&& other) = default;

VizMainImpl::VizMainImpl(Delegate* delegate,
                         ExternalDependencies dependencies,
                         std::unique_ptr<gpu::GpuInit> gpu_init)
    : delegate_(delegate),
      dependencies_(std::move(dependencies)),
      gpu_init_(std::move(gpu_init)),
      gpu_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(gpu_init_);

  // TODO(crbug.com/609317): Remove this when Mus Window Server and GPU are
  // split into separate processes. Until then this is necessary to be able to
  // run Mushrome (chrome with mus) with Mus running in the browser process.
  if (dependencies_.power_monitor_source) {
    base::PowerMonitor::Initialize(
        std::move(dependencies_.power_monitor_source));
  }

  if (!dependencies_.io_thread_task_runner)
    io_thread_ = CreateAndStartIOThread();

  if (dependencies_.viz_compositor_thread_runner) {
    viz_compositor_thread_runner_ = dependencies_.viz_compositor_thread_runner;
  } else {
    viz_compositor_thread_runner_impl_ =
        std::make_unique<VizCompositorThreadRunnerImpl>();
    viz_compositor_thread_runner_ = viz_compositor_thread_runner_impl_.get();
  }
  if (delegate_) {
    delegate_->PostCompositorThreadCreated(
        viz_compositor_thread_runner_->task_runner());
  }

  if (!gpu_init_->gpu_info().in_process_gpu && dependencies_.ukm_recorder) {
    // NOTE: If the GPU is running in the browser process, we can use the
    // browser's UKMRecorder.
    ukm::DelegatingUkmRecorder::Get()->AddDelegate(
        dependencies_.ukm_recorder->GetWeakPtr());
  }

  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu_init_->gpu_info(), gpu_init_->TakeWatchdogThread(), io_task_runner(),
      gpu_init_->gpu_feature_info(), gpu_init_->gpu_preferences(),
      gpu_init_->gpu_info_for_hardware_gpu(),
      gpu_init_->gpu_feature_info_for_hardware_gpu(),
      gpu_init_->gpu_extra_info(), gpu_init_->vulkan_implementation(),
      base::BindOnce(&VizMainImpl::ExitProcess, base::Unretained(this)));
}

VizMainImpl::~VizMainImpl() {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  // The compositor holds on to some resources from gpu service. So destroy the
  // compositor first, before destroying the gpu service. However, before the
  // compositor is destroyed, close the binding, so that the gpu service doesn't
  // need to process commands from the host as it is shutting down.
  receiver_.reset();

  // If the VizCompositorThread was started and owned by VizMainImpl, then this
  // will block until the thread has been shutdown. All RootCompositorFrameSinks
  // must be destroyed before now, otherwise the compositor thread will deadlock
  // waiting for a response from the blocked GPU thread.
  // For the non-owned case for Android WebView, Viz does not communicate with
  // this thread so there is no need to shutdown viz first.
  viz_compositor_thread_runner_ = nullptr;
  viz_compositor_thread_runner_impl_.reset();

  if (dependencies_.ukm_recorder)
    ukm::DelegatingUkmRecorder::Get()->RemoveDelegate(
        dependencies_.ukm_recorder.get());
}

void VizMainImpl::BindAssociated(
    mojo::PendingAssociatedReceiver<mojom::VizMain> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void VizMainImpl::CreateGpuService(
    mojo::PendingReceiver<mojom::GpuService> pending_receiver,
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager>
        discardable_memory_manager,
    mojo::ScopedSharedBufferHandle activity_flags,
    gfx::FontRenderParams::SubpixelRendering subpixel_rendering) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  mojo::Remote<mojom::GpuHost> gpu_host(std::move(pending_gpu_host));

  // If GL is disabled then don't try to collect GPUInfo, we're not using GPU.
  if (gl::GetGLImplementation() != gl::kGLImplementationDisabled)
    gpu_service_->UpdateGPUInfo();

  if (!gpu_init_->init_successful()) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    GpuServiceImpl::FlushPreInitializeLogMessages(gpu_host.get());
    gpu_service_.reset();
    gpu_host->DidFailInitialize();
    if (delegate_)
      delegate_->OnInitializationFailed();
    return;
  }

  if (!gpu_init_->gpu_info().in_process_gpu) {
    // If the GPU is running in the browser process, discardable memory manager
    // has already been initialized.
    discardable_shared_memory_manager_ = base::MakeRefCounted<
        discardable_memory::ClientDiscardableSharedMemoryManager>(
        std::move(discardable_memory_manager), io_task_runner());
    base::DiscardableMemoryAllocator::SetInstance(
        discardable_shared_memory_manager_.get());
  }

  skia::LegacyDisplayGlobals::SetCachedPixelGeometry(
      gfx::FontRenderParams::SubpixelRenderingToSkiaPixelGeometry(
          subpixel_rendering));

  gpu_service_->Bind(std::move(pending_receiver));
  gpu_service_->InitializeWithHost(
      gpu_host.Unbind(),
      gpu::GpuProcessActivityFlags(std::move(activity_flags)),
      gpu_init_->TakeDefaultOffscreenSurface(),
      dependencies_.sync_point_manager, dependencies_.shared_image_manager,
      dependencies_.shutdown_event);

  if (!pending_frame_sink_manager_params_.is_null()) {
    CreateFrameSinkManagerInternal(
        std::move(pending_frame_sink_manager_params_));
    pending_frame_sink_manager_params_.reset();
  }
  if (delegate_)
    delegate_->OnGpuServiceConnection(gpu_service_.get());
}

#if defined(OS_WIN)
void VizMainImpl::CreateInfoCollectionGpuService(
    mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!info_collection_gpu_service_);
  DCHECK(gpu_init_->device_perf_info().has_value());

  info_collection_gpu_service_ = std::make_unique<InfoCollectionGpuServiceImpl>(
      gpu_thread_task_runner_, io_task_runner(),
      gpu_init_->device_perf_info().value(), gpu_init_->gpu_info().active_gpu(),
      std::move(pending_receiver));
}
#endif

void VizMainImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(viz_compositor_thread_runner_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (!gpu_service_ || !gpu_service_->is_initialized()) {
    DCHECK(pending_frame_sink_manager_params_.is_null());
    pending_frame_sink_manager_params_ = std::move(params);
    return;
  }
  CreateFrameSinkManagerInternal(std::move(params));
}

void VizMainImpl::CreateFrameSinkManagerInternal(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(gpu_service_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  gl::GLSurfaceFormat format;
  // If we are running a SW Viz process, we may not have a default offscreen
  // surface.
  if (auto* offscreen_surface =
          gpu_service_->gpu_channel_manager()->default_offscreen_surface()) {
    format = offscreen_surface->GetFormat();
  } else {
    DCHECK_EQ(gl::GetGLImplementation(), gl::kGLImplementationDisabled);
  }

  // When the host loses its connection to the viz process, it assumes the
  // process has crashed and tries to reinitialize it. However, it is possible
  // to have lost the connection for other reasons (e.g. deserialization
  // errors) and the viz process is already set up. We cannot recreate
  // FrameSinkManagerImpl, so just do a hard CHECK rather than crashing down the
  // road so that all crash reports caused by this issue look the same and have
  // the same signature. https://crbug.com/928845
  CHECK(!task_executor_);

  task_executor_ = std::make_unique<gpu::GpuInProcessThreadService>(
      this, gpu_thread_task_runner_, gpu_service_->GetGpuScheduler(),
      gpu_service_->sync_point_manager(), gpu_service_->mailbox_manager(),
      format, gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences(),
      gpu_service_->shared_image_manager(),
      gpu_service_->gpu_channel_manager()->program_cache());

  viz_compositor_thread_runner_->CreateFrameSinkManager(
      std::move(params), task_executor_.get(), gpu_service_.get());
}

void VizMainImpl::CreateVizDevTools(mojom::VizDevToolsParamsPtr params) {
#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  viz_compositor_thread_runner_->CreateVizDevTools(std::move(params));
#endif
}

scoped_refptr<gpu::SharedContextState> VizMainImpl::GetSharedContextState() {
  return gpu_service_->GetContextState();
}

scoped_refptr<gl::GLShareGroup> VizMainImpl::GetShareGroup() {
  return gpu_service_->share_group();
}

void VizMainImpl::ExitProcess(base::Optional<ExitCode> immediate_exit_code) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  if (!gpu_init_->gpu_info().in_process_gpu && immediate_exit_code) {
    // Atomically shut down GPU process to make it faster and simpler.
    base::Process::TerminateCurrentProcessImmediately(
        static_cast<int>(immediate_exit_code.value()));
    return;
  }

  // Close mojom::VizMain bindings first so the browser can't try to reconnect.
  receiver_.reset();

  if (viz_compositor_thread_runner_) {
    // Destroy RootCompositorFrameSinkImpls on the compositor while the GPU
    // thread is still running to avoid deadlock. Quit GPU thread TaskRunner
    // after cleanup on compositor thread is finished.
    viz_compositor_thread_runner_->CleanupForShutdown(base::BindOnce(
        &Delegate::QuitMainMessageLoop, base::Unretained(delegate_)));
  } else {
    delegate_->QuitMainMessageLoop();
  }
}

}  // namespace viz
