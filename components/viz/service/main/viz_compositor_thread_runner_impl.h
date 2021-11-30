// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/service/main/viz_compositor_thread_runner.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/java_handler_thread.h"
#endif

namespace base {
class Thread;
}  // namespace base

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

  VizCompositorThreadRunnerImpl(const VizCompositorThreadRunnerImpl&) = delete;
  VizCompositorThreadRunnerImpl& operator=(
      const VizCompositorThreadRunnerImpl&) = delete;

  // Performs teardown on thread and then stops thread.
  ~VizCompositorThreadRunnerImpl() override;

  // VizCompositorThreadRunner overrides.
  base::SingleThreadTaskRunner* task_runner() override;
  base::PlatformThreadId thread_id() override;
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params) override;
  void CreateFrameSinkManager(
      mojom::FrameSinkManagerParamsPtr params,
      gpu::CommandBufferTaskExecutor* task_executor,
      GpuServiceImpl* gpu_service,
      HintSessionFactory* hint_session_factory) override;

 private:
  void CreateFrameSinkManagerOnCompositorThread(
      mojom::FrameSinkManagerParamsPtr params,
      gpu::CommandBufferTaskExecutor* task_executor,
      GpuServiceImpl* gpu_service,
      HintSessionFactory* hint_session_factory);
  void TearDownOnCompositorThread();

  // Start variables to be accessed only on |task_runner_|.
  std::unique_ptr<ServerSharedBitmapManager> server_shared_bitmap_manager_;
  std::unique_ptr<OutputSurfaceProvider> output_surface_provider_;
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
  // End variables to be accessed only on |task_runner_|.

  std::unique_ptr<VizCompositorThreadType> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_
