// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/ui_devtools/buildflags.h"
#include "components/viz/service/main/viz_compositor_thread_runner.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/java_handler_thread.h"
#endif

namespace base {
class Thread;
}  // namespace base

namespace ui_devtools {
class UiDevToolsServer;
}  // namespace ui_devtools

namespace viz {
class OutputSurfaceProvider;
class FrameSinkManagerImpl;
class ServerSharedBitmapManager;

#if defined(OS_ANDROID)
using VizCompositorThreadType = base::android::JavaHandlerThread;
#else
using VizCompositorThreadType = base::Thread;
#endif

class VizCompositorThreadRunnerImpl : public VizCompositorThreadRunner {
 public:
  VizCompositorThreadRunnerImpl();
  // Performs teardown on thread and then stops thread.
  ~VizCompositorThreadRunnerImpl() override;

  // VizCompositorThreadRunner overrides.
  base::SingleThreadTaskRunner* task_runner() override;
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params) override;
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params,
                              gpu::CommandBufferTaskExecutor* task_executor,
                              GpuServiceImpl* gpu_service) override;
#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  void CreateVizDevTools(mojom::VizDevToolsParamsPtr params) override;
#endif
  void CleanupForShutdown(base::OnceClosure cleanup_finished_callback) override;

 private:
  void CreateFrameSinkManagerOnCompositorThread(
      mojom::FrameSinkManagerParamsPtr params,
      gpu::CommandBufferTaskExecutor* task_executor,
      GpuServiceImpl* gpu_service);
#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  void CreateVizDevToolsOnCompositorThread(mojom::VizDevToolsParamsPtr params);
  void InitVizDevToolsOnCompositorThread(mojom::VizDevToolsParamsPtr params);
#endif
  void CleanupForShutdownOnCompositorThread();
  void TearDownOnCompositorThread();

  // Start variables to be accessed only on |task_runner_|.
  std::unique_ptr<ServerSharedBitmapManager> server_shared_bitmap_manager_;
  std::unique_ptr<OutputSurfaceProvider> output_surface_provider_;
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  std::unique_ptr<ui_devtools::UiDevToolsServer> devtools_server_;

  // If the FrameSinkManager is not ready yet, then we stash the pending
  // VizDevToolsParams.
  mojom::VizDevToolsParamsPtr pending_viz_dev_tools_params_;
#endif
  // End variables to be accessed only on |task_runner_|.

  std::unique_ptr<VizCompositorThreadType> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(VizCompositorThreadRunnerImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_
