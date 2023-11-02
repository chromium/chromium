// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_DEMO_HOST_DEMO_HOST_H_
#define COMPONENTS_VIZ_DEMO_HOST_DEMO_HOST_H_

#include <memory>
#include <vector>

#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace demo {

class DemoClient;

// DemoHost is the 'host', i.e. the privileged component, responsible for
// managing the service, and establishing the connection between the clients and
// the service.
class DemoHost : public viz::HostFrameSinkClient {
 public:
  DemoHost(
      gfx::AcceleratedWidget widget,
      const gfx::Size& size,
      mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient> client_receiver,
      mojo::PendingRemote<viz::mojom::FrameSinkManager>
          frame_sink_manager_remote);

  DemoHost(const DemoHost&) = delete;
  DemoHost& operator=(const DemoHost&) = delete;

  ~DemoHost() override;

  void Resize(const gfx::Size& size);

 private:
  void ResizeOnThread(const gfx::Size& size);

  void EmbedClients(DemoClient* embedder_client, const gfx::Rect& child_bounds);

  void Initialize(
      mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient> receiver,
      mojo::PendingRemote<viz::mojom::FrameSinkManager> remote);

  // viz::HostFrameSinkClient:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  const gfx::AcceleratedWidget widget_;
  gfx::Size size_;
  viz::HostFrameSinkManager host_frame_sink_manager_;
  mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private_;
  std::unique_ptr<viz::HostDisplayClient> display_client_;
  viz::ParentLocalSurfaceIdAllocator allocator_;

  std::unique_ptr<DemoClient> root_client_;
  std::vector<std::unique_ptr<DemoClient>> embedded_clients_;

  // The thread is created to demonstrate that the host can run on a separate
  // thread (or even a separate process), since it communicates with the service
  // and clients over mojo. The host does not need to have its own thread
  // though.
  base::Thread thread_;
};

}  // namespace demo

#endif  // COMPONENTS_VIZ_DEMO_HOST_DEMO_HOST_H_
