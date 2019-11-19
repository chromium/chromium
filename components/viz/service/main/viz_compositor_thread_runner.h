// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_

#include "base/callback.h"
#include "components/ui_devtools/buildflags.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
class CommandBufferTaskExecutor;
}  // namespace gpu

namespace viz {

class GpuServiceImpl;

// Starts and runs the VizCompositorThread. The thread will be started when this
// object is constructed. Objects on the thread will be initialized after
// calling CreateFrameSinkManager(). Destructor will teardown objects on thread
// and then stop the thread.
class VizCompositorThreadRunner {
 public:
  virtual ~VizCompositorThreadRunner() = default;

  // Returns the TaskRunner for VizCompositorThread.
  virtual base::SingleThreadTaskRunner* task_runner() = 0;

  // Creates FrameSinkManager from |params|. The version with |gpu_service| and
  // |task_executor| supports both GPU and software compositing, while the
  // version without supports only software compositing. Should be called from
  // the thread that owns |this| to initialize state on VizCompositorThread.
  virtual void CreateFrameSinkManager(
      mojom::FrameSinkManagerParamsPtr params) = 0;
  virtual void CreateFrameSinkManager(
      mojom::FrameSinkManagerParamsPtr params,
      gpu::CommandBufferTaskExecutor* task_executor,
      GpuServiceImpl* gpu_service) = 0;

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  virtual void CreateVizDevTools(mojom::VizDevToolsParamsPtr params) = 0;
#endif

  // Performs cleanup on VizCompositorThread needed before forcing thread to
  // shut down. Ensures VizCompositorThread teardown during the destructor
  // doesn't block on PostTasks back to the GPU thread. After cleanup has
  // finished |cleanup_finished_callback| will be run. Should be called from the
  // thread that owns |this|.
  //
  // This is intended to be used when the GPU thread wants to force restart. The
  // cleanup is normally handled by the browser process before GPU process
  // shutdown, except if the GPU thread is forcing restart.
  virtual void CleanupForShutdown(
      base::OnceClosure cleanup_finished_callback) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
