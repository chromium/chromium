// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_HOST_FRAME_SINK_CLIENT_H_
#define COMPONENTS_VIZ_TEST_FAKE_HOST_FRAME_SINK_CLIENT_H_

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"

namespace viz {

// HostFrameSinkClient implementation that does nothing.
class FakeHostFrameSinkClient : public HostFrameSinkClient {
 public:
  FakeHostFrameSinkClient();

  FakeHostFrameSinkClient(const FakeHostFrameSinkClient&) = delete;
  FakeHostFrameSinkClient& operator=(const FakeHostFrameSinkClient&) = delete;

  ~FakeHostFrameSinkClient() override;

  // HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override {}
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;
  uint32_t last_frame_token_seen() const { return last_frame_token_seen_; }

 private:
  uint32_t last_frame_token_seen_ = kInvalidFrameToken;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_HOST_FRAME_SINK_CLIENT_H_
