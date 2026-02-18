// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_BROWSER_DUMMY_SURFACE_PROVIDER_H_
#define COMPONENTS_SURFACE_EMBED_BROWSER_DUMMY_SURFACE_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace surface_embed {

// A dummy surface provider that owns a FrameSinkId and draws a solid red
// rectangle on the surface. This is used in tests by the SurfaceEmbedHost to
// avoid the need for a real embedder surface.
class DummySurfaceProvider : public viz::HostFrameSinkClient,
                             public viz::mojom::CompositorFrameSinkClient {
 public:
  DummySurfaceProvider();
  ~DummySurfaceProvider() override;

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                             float device_scale_factor,
                             const gfx::Size& frame_size_in_pixels);

  // viz::HostFrameSinkClient implementation:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  // viz::mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFrame(
      const viz::BeginFrameArgs& args,
      const base::flat_map<uint32_t, viz::FrameTimingDetails>& details,
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override;
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override;

 private:
  raw_ptr<viz::HostFrameSinkManager> frame_sink_manager_;
  viz::FrameTokenGenerator frame_token_generator_;
  viz::FrameSinkId frame_sink_id_;
  mojo::Remote<viz::mojom::CompositorFrameSink> compositor_frame_sink_remote_;
  mojo::Receiver<viz::mojom::CompositorFrameSinkClient>
      compositor_frame_sink_client_receiver_{this};
};

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_BROWSER_DUMMY_SURFACE_PROVIDER_H_
