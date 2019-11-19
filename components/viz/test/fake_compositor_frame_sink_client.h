// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_COMPOSITOR_FRAME_SINK_CLIENT_H_
#define COMPONENTS_VIZ_TEST_FAKE_COMPOSITOR_FRAME_SINK_CLIENT_H_

#include <vector>

#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace viz {

class FakeCompositorFrameSinkClient : public mojom::CompositorFrameSinkClient {
 public:
  FakeCompositorFrameSinkClient();
  ~FakeCompositorFrameSinkClient() override;

  mojo::PendingRemote<mojom::CompositorFrameSinkClient> BindInterfaceRemote();

  // mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<ReturnedResource>& resources) override;
  void OnBeginFrame(const BeginFrameArgs& args,
                    const FrameTimingDetailsMap& timing_details) override;
  void ReclaimResources(
      const std::vector<ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  void clear_returned_resources() { returned_resources_.clear(); }
  const std::vector<ReturnedResource>& returned_resources() const {
    return returned_resources_;
  }

 private:
  void InsertResources(const std::vector<ReturnedResource>& resources);

  std::vector<ReturnedResource> returned_resources_;

  mojo::Receiver<mojom::CompositorFrameSinkClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorFrameSinkClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_COMPOSITOR_FRAME_SINK_CLIENT_H_
