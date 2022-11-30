// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/mock_compositor_frame_sink_client.h"

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

MockCompositorFrameSinkClient::MockCompositorFrameSinkClient() = default;
MockCompositorFrameSinkClient::~MockCompositorFrameSinkClient() = default;

mojo::PendingRemote<mojom::CompositorFrameSinkClient>
MockCompositorFrameSinkClient::BindInterfaceRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace viz
