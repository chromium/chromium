// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/service/main/viz_compositor_thread_runner.h"
#include "gpu/command_buffer/service/shared_context_state.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/java_handler_thread.h"
#endif

namespace base {
class Thread;
class WaitableEvent;
}  // namespace base

namespace viz {
class FrameSinkManagerImpl;
class GmbVideoFramePoolContextProvider;
class HintSessionFactory;
class InProcessGpuMemoryBufferManager;
class OutputSurfaceProvider;
class ServerSharedBitmapManager;
class SharedImageInterfaceProvider;

#if BUILDFLAG(IS_ANDROID)
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
  bool CreateHintSessionFactory(
      base::flat_set<base::PlatformThreadId> thread_ids,
      base::RepeatingClosure* wake_up_closure) override;
  void SetIOThreadId(base::PlatformThreadId io_thread_id) override {}
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params,
                              GpuServiceImpl* gpu_service) override;
  void RequestBeginFrameForGpuService(bool toggle) override;

 private:
  void CreateHintSessionFactoryOnCompositorThread(
      base::flat_set<base::PlatformThreadId> thread_ids,
      base::RepeatingClosure* wake_up_closure,
      base::WaitableEvent* event);
  void WakeUpOnCompositorThread();
  void CreateFrameSinkManagerOnCompositorThread(
      mojom::FrameSinkManagerParamsPtr params,
      GpuServiceImpl* gpu_service);
  void RequestBeginFrameForGpuServiceOnCompositorThread(bool toggle);
  void TearDownOnCompositorThread();

  std::unique_ptr<VizCompositorThreadType> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  std::unique_ptr<SharedImageInterfaceProvider>
      shared_image_interface_provider_;

  // Start variables to be accessed only on |task_runner_|.
  std::unique_ptr<HintSessionFactory> hint_session_factory_;
  std::unique_ptr<ServerSharedBitmapManager> server_shared_bitmap_manager_;
  std::unique_ptr<InProcessGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<OutputSurfaceProvider> output_surface_provider_;
  // `gmb_video_frame_pool_context_provider_` depends on
  // `gpu_memory_buffer_manager_`. It must be created last, deleted first.
  std::unique_ptr<GmbVideoFramePoolContextProvider>
      gmb_video_frame_pool_context_provider_;
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
  base::WeakPtrFactory<VizCompositorThreadRunnerImpl> weak_factory_{this};
  // End variables to be accessed only on |task_runner_|.
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_IMPL_H_
