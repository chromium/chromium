// Copyright 2017 The Chromium Authors
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

  FakeCompositorFrameSinkClient(const FakeCompositorFrameSinkClient&) = delete;
  FakeCompositorFrameSinkClient& operator=(
      const FakeCompositorFrameSinkClient&) = delete;

  ~FakeCompositorFrameSinkClient() override;

  mojo::PendingRemote<mojom::CompositorFrameSinkClient> BindInterfaceRemote();

  // mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      std::vector<ReturnedResource> resources) override;
  void OnBeginFrame(const BeginFrameArgs& args,
                    const FrameTimingDetailsMap& timing_details,
                    bool frame_ack,
                    std::vector<ReturnedResource> resources) override;
  void ReclaimResources(std::vector<ReturnedResource> resources) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}
  void OnSurfaceEvicted(const LocalSurfaceId& local_surface_id) override {}

  void clear_returned_resources() { returned_resources_.clear(); }
  const std::vector<ReturnedResource>& returned_resources() const {
    return returned_resources_;
  }

  void clear_begin_frame_count() { begin_frame_count_ = 0; }
  int begin_frame_count() const { return begin_frame_count_; }

  const FrameTimingDetailsMap& all_frame_timing_details() const {
    return all_frame_timing_details_;
  }

 private:
  void InsertResources(std::vector<ReturnedResource> resources);

  int begin_frame_count_ = 0;
  std::vector<ReturnedResource> returned_resources_;

  mojo::Receiver<mojom::CompositorFrameSinkClient> receiver_{this};

  FrameTimingDetailsMap all_frame_timing_details_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_COMPOSITOR_FRAME_SINK_CLIENT_H_
