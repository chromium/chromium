// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_compositor_frame_sink_client.h"

namespace viz {

FakeCompositorFrameSinkClient::FakeCompositorFrameSinkClient() = default;
FakeCompositorFrameSinkClient::~FakeCompositorFrameSinkClient() = default;

void FakeCompositorFrameSinkClient::DidReceiveCompositorFrameAck(
    std::vector<ReturnedResource> resources) {
  InsertResources(std::move(resources));
}

void FakeCompositorFrameSinkClient::OnBeginFrame(
    const BeginFrameArgs& args,
    const FrameTimingDetailsMap& timing_details) {}

void FakeCompositorFrameSinkClient::ReclaimResources(
    std::vector<ReturnedResource> resources) {
  InsertResources(std::move(resources));
}

void FakeCompositorFrameSinkClient::OnBeginFramePausedChanged(bool paused) {}

void FakeCompositorFrameSinkClient::InsertResources(
    std::vector<ReturnedResource> resources) {
  returned_resources_.insert(returned_resources_.end(),
                             std::make_move_iterator(resources.begin()),
                             std::make_move_iterator(resources.end()));
}

mojo::PendingRemote<mojom::CompositorFrameSinkClient>
FakeCompositorFrameSinkClient::BindInterfaceRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace viz
