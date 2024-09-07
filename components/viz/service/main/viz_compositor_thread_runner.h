// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/threading/platform_thread.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

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
  virtual bool CreateHintSessionFactory(
      base::flat_set<base::PlatformThreadId> thread_ids,
      base::RepeatingClosure* wake_up_closure) = 0;
  virtual void SetIOThreadId(base::PlatformThreadId io_thread_id) = 0;

  // Creates FrameSinkManager from |params|. If |gpu_service| is null the
  // display compositor will only support software compositing. Should be called
  // from the thread that owns |this| to initialize state on
  // VizCompositorThread.
  virtual void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params,
                                      GpuServiceImpl* gpu_service) = 0;
  virtual void RequestBeginFrameForGpuService(bool toggle) {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
