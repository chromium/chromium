// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_MOCK_COMPOSITOR_FRAME_SINK_CLIENT_H_
#define COMPONENTS_VIZ_TEST_MOCK_COMPOSITOR_FRAME_SINK_CLIENT_H_

#include "base/callback.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace viz {

class MockCompositorFrameSinkClient : public mojom::CompositorFrameSinkClient {
 public:
  MockCompositorFrameSinkClient();
  ~MockCompositorFrameSinkClient() override;

  void set_disconnect_handler(base::OnceClosure error_handler) {
    receiver_.set_disconnect_handler(std::move(error_handler));
  }

  // Returns a mojo::PendingRemote<CompositorFrameSinkClient> bound to this
  // object.
  mojo::PendingRemote<mojom::CompositorFrameSinkClient> BindInterfaceRemote();

  // mojom::CompositorFrameSinkClient implementation.
  MOCK_METHOD1(DidReceiveCompositorFrameAck,
               void(const std::vector<ReturnedResource>&));
  MOCK_METHOD2(OnBeginFrame,
               void(const BeginFrameArgs&, const FrameTimingDetailsMap&));
  MOCK_METHOD1(ReclaimResources, void(const std::vector<ReturnedResource>&));
  MOCK_METHOD2(WillDrawSurface, void(const LocalSurfaceId&, const gfx::Rect&));
  MOCK_METHOD1(OnBeginFramePausedChanged, void(bool paused));

 private:
  mojo::Receiver<mojom::CompositorFrameSinkClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(MockCompositorFrameSinkClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_MOCK_COMPOSITOR_FRAME_SINK_CLIENT_H_
