// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/demo/service/demo_service.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/demo/common/switches.h"
#include "components/viz/service/main/viz_compositor_thread_runner_impl.h"
#include "gpu/ipc/service/gpu_init.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace demo {

namespace {

std::unique_ptr<base::Thread> CreateAndStartIOThread() {
  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  thread_options.thread_type = base::ThreadType::kDisplayCritical;
  auto io_thread = std::make_unique<base::Thread>("VizDemoGpuIOThread");
  CHECK(io_thread->StartWithOptions(std::move(thread_options)));
  return io_thread;
}

}  // namespace

DemoService::DemoService(
    mojo::PendingReceiver<viz::mojom::FrameSinkManager> receiver,
    mojo::PendingRemote<viz::mojom::FrameSinkManagerClient> client) {
  auto params = viz::mojom::FrameSinkManagerParams::New();
  params->restart_id = viz::BeginFrameSource::kNotRestartableId;
  params->use_activation_deadline = false;
  params->activation_deadline_in_frames = 0u;
  params->frame_sink_manager = std::move(receiver);
  params->frame_sink_manager_client = std::move(client);
  runner_ = std::make_unique<viz::VizCompositorThreadRunnerImpl>();

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kVizDemoUseGPU)) {
    gpu_init_ = std::make_unique<gpu::GpuInit>();

    io_thread_ = CreateAndStartIOThread();

    viz::GpuServiceImpl::InitParams init_params;
    init_params.watchdog_thread = gpu_init_->TakeWatchdogThread();
    init_params.io_runner = io_thread_->task_runner();
    init_params.vulkan_implementation = gpu_init_->vulkan_implementation();
    init_params.exit_callback =
        base::BindOnce(&DemoService::ExitProcess, base::Unretained(this));

    gpu_service_ = std::make_unique<viz::GpuServiceImpl>(
        gpu_init_->gpu_preferences(), gpu_init_->gpu_info(),
        gpu_init_->gpu_feature_info(), gpu_init_->gpu_info_for_hardware_gpu(),
        gpu_init_->gpu_feature_info_for_hardware_gpu(),
        gpu_init_->gpu_extra_info(), std::move(init_params));

    mojo::PendingRemote<viz::mojom::GpuHost> gpu_host_proxy;
    std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();

    gpu_service_->InitializeWithHost(
        std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
        gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                           gfx::Size()),
        /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
        /*scheduler=*/nullptr, /*shutdown_event=*/nullptr);
  }

  runner_->CreateFrameSinkManager(std::move(params), gpu_service_.get());
}

DemoService::~DemoService() = default;

void DemoService::ExitProcess(viz::ExitCode immediate_exit_code) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace demo
