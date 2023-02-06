// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/demo/host/demo_host.h"

#include <utility>

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/demo/client/demo_client.h"
#include "components/viz/demo/common/switches.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace demo {

DemoHost::DemoHost(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient> client_receiver,
    mojo::PendingRemote<viz::mojom::FrameSinkManager> frame_sink_manager_remote)
    : widget_(widget), size_(size), thread_("DemoHost") {
  CHECK(thread_.Start());
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DemoHost::Initialize, base::Unretained(this),
                                std::move(client_receiver),
                                std::move(frame_sink_manager_remote)));
}

DemoHost::~DemoHost() = default;

void DemoHost::Resize(const gfx::Size& size) {
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DemoHost::ResizeOnThread, base::Unretained(this), size));
}

void DemoHost::ResizeOnThread(const gfx::Size& size) {
  if (size_ == size)
    return;
  size_ = size;
  display_private_->Resize(size);

  // Every size change for a client needs a new LocalSurfaceId.
  allocator_.GenerateId();
  root_client_->Resize(size_, allocator_.GetCurrentLocalSurfaceId());
}

void DemoHost::EmbedClients(DemoClient* embedder_client,
                            const gfx::Rect& child_bounds) {
  // Generate a FrameSinkId for the client. Each client can have any number of
  // frame-sinks, and these frame-sinks should share the same |client_id|
  // component for the FrameSinkId. In this demo however, each client has a
  // single FrameSink, so the client-id can just be randomly generated, and it
  // doesn't make a difference.
  uint64_t rand = base::RandUint64();
  viz::FrameSinkId frame_sink_id(rand >> 32, rand & 0xffffffff);

  // Register the frame sink and its hierarchy.
  host_frame_sink_manager_.RegisterFrameSinkId(
      frame_sink_id, this, viz::ReportFirstSurfaceActivation::kNo);
  host_frame_sink_manager_.RegisterFrameSinkHierarchy(
      embedder_client->frame_sink_id(), frame_sink_id);

  // Next, create a mojom::CompositorFrameSink for the client, so that the
  // client is able to submit visual (and hit-test) content to the viz service.
  // Note that in this demo app, the host is setting up the message-pipes, and
  // then sending the end-points to the embedded-client and the viz-service.
  // However, it is possible for the embedded-client to initiate the creation of
  // the message-pipes, in which case, the client would need to send the
  // service-end-points to the host (via a non-viz API), so that the host can in
  // turn send them to the service.
  mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote;
  auto sink_receiver = sink_remote.InitWithNewPipeAndPassReceiver();

  host_frame_sink_manager_.CreateCompositorFrameSink(
      frame_sink_id, std::move(sink_receiver), std::move(client_remote));

  // At this point, the host is done setting everything up. Now it is up to the
  // new client to take over the communication (i.e. the mojo message pipes)
  // with the service for the frame-sink. The embedder (i.e. the parent client)
  // also needs to know about the new client's FrameSinkId, so that it is able
  // to embed it. Both the embedder and the embedded client also need to use
  // the same LocalSurfaceId for the embedding. Typically, the embedder is the
  // one that generates the LocalSurfaceId for the embedded client. However, it
  // is possible for another source (e.g. the viz-host) to generate the
  // LocalSurfaceId, and dispatch that separately to both the embedder and the
  // embedded clients. There is no specific viz-API for communicating these
  // FrameSinkId and LocalSurfaceId between these clients. In chrome, these
  // happen through the content API (or through the window-service API in
  // ChromeOS).
  // In this demo app, the embedder-client is assigning the LocalSurfaceId
  // (through DemoClient).

  auto lsid_allocation = embedder_client->Embed(frame_sink_id, child_bounds);
  auto embedded_client = std::make_unique<DemoClient>(
      frame_sink_id, lsid_allocation, child_bounds);
  embedded_client->Initialize(std::move(client_receiver),
                              std::move(sink_remote));
  if (embedder_client == root_client_.get()) {
    // Embed another client after a second. This could embed the client
    // immediately here too if desired. The delay is to demonstrate asynchronous
    // usage of the API.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DemoHost::EmbedClients, base::Unretained(this),
                       embedded_client.get(), gfx::Rect(125, 125, 150, 150)),
        base::Seconds(1));
  }
  embedded_clients_.push_back(std::move(embedded_client));
}

void DemoHost::Initialize(
    mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient> receiver,
    mojo::PendingRemote<viz::mojom::FrameSinkManager> remote) {
  host_frame_sink_manager_.BindAndSetManager(std::move(receiver), nullptr,
                                             std::move(remote));

  display_client_ = std::make_unique<viz::HostDisplayClient>(widget_);

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

  // Create interfaces for a root CompositorFrameSink.
  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  root_params->compositor_frame_sink =
      sink_remote.InitWithNewEndpointAndPassReceiver();
  auto client_receiver = root_params->compositor_frame_sink_client
                             .InitWithNewPipeAndPassReceiver();
  root_params->display_private =
      display_private_.BindNewEndpointAndPassReceiver();
  root_params->display_client = display_client_->GetBoundRemote(nullptr);

  constexpr viz::FrameSinkId root_frame_sink_id(0xdead, 0xbeef);
  root_params->frame_sink_id = root_frame_sink_id;
  root_params->widget = widget_;

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  root_params->gpu_compositing = cmd_line->HasSwitch(switches::kVizDemoUseGPU);

  root_params->renderer_settings = viz::CreateRendererSettings();

  host_frame_sink_manager_.RegisterFrameSinkId(
      root_params->frame_sink_id, this, viz::ReportFirstSurfaceActivation::kNo);
  host_frame_sink_manager_.CreateRootCompositorFrameSink(
      std::move(root_params));

  display_private_->Resize(size_);
  display_private_->SetDisplayVisible(true);

  // Initialize as a client now, since the host has to submit compositor frames
  // like any other clients.
  // The 'root' is not embedded by anything else. However, it still needs to
  // have a valid LocalSurfaceId, which changes when root changes size (or
  // device-scale-factor etc.).
  allocator_.GenerateId();
  root_client_ = std::make_unique<DemoClient>(
      root_frame_sink_id, allocator_.GetCurrentLocalSurfaceId(),
      gfx::Rect(size_));
  root_client_->Initialize(std::move(client_receiver), std::move(sink_remote));

  // Embed a new client into the root after the first second.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DemoHost::EmbedClients, base::Unretained(this),
                     root_client_.get(), gfx::Rect(50, 50, 300, 300)),
      base::Seconds(1));
}

void DemoHost::OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) {}

void DemoHost::OnFrameTokenChanged(uint32_t frame_token,
                                   base::TimeTicks activation_time) {}

}  // namespace demo
