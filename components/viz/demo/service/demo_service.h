// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_DEMO_SERVICE_DEMO_SERVICE_H_
#define COMPONENTS_VIZ_DEMO_SERVICE_DEMO_SERVICE_H_

#include <memory>

#include "base/threading/thread.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"

namespace viz {
class VizCompositorThreadRunnerImpl;
}  // namespace viz

namespace gpu {
class GpuInit;
}

namespace base {
class Thread;
}

namespace demo {

// DemoService sets up the display compositor, and connects to the host over the
// mojom API. The host communicates with the FrameSinkManagerImpl over the
// mojom.FrameSinkManager API, and the FrameSinkManagerImpl communicates with
// the host over the mojom.FrameSinkManagerClient API.
class DemoService {
 public:
  DemoService(mojo::PendingReceiver<viz::mojom::FrameSinkManager> receiver,
              mojo::PendingRemote<viz::mojom::FrameSinkManagerClient> client);

  DemoService(const DemoService&) = delete;
  DemoService& operator=(const DemoService&) = delete;

  ~DemoService();

 private:
  void ExitProcess(viz::ExitCode immediate_exit_code);

  std::unique_ptr<viz::VizCompositorThreadRunnerImpl> runner_;

  std::unique_ptr<base::Thread> io_thread_;

  std::unique_ptr<gpu::GpuInit> gpu_init_;
  std::unique_ptr<viz::GpuServiceImpl> gpu_service_;
};

}  // namespace demo

#endif  // COMPONENTS_VIZ_DEMO_SERVICE_DEMO_SERVICE_H_
