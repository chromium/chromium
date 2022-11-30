// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_STUB_SURFACE_CLIENT_H_
#define COMPONENTS_VIZ_TEST_STUB_SURFACE_CLIENT_H_

#include <vector>

#include "components/viz/service/surfaces/surface_client.h"

#include "base/memory/weak_ptr.h"

namespace viz {

class StubSurfaceClient : public SurfaceClient {
 public:
  StubSurfaceClient();
  ~StubSurfaceClient() override;

  void OnSurfaceCommitted(Surface* surface) override {}
  void OnSurfaceActivated(Surface* surface) override {}
  void OnSurfaceDestroyed(Surface* surface) override {}
  void OnSurfaceWillDraw(Surface* surface) override {}
  void RefResources(
      const std::vector<TransferableResource>& resources) override {}
  void UnrefResources(std::vector<ReturnedResource> resources) override {}
  void ReturnResources(std::vector<ReturnedResource> resources) override {}
  void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) override {}
  std::vector<PendingCopyOutputRequest> TakeCopyOutputRequests(
      const LocalSurfaceId& latest_surface_id) override;
  void OnFrameTokenChanged(uint32_t frame_token) override {}
  void SendCompositorFrameAck() override {}
  void OnSurfaceAggregatedDamage(
      Surface* surface,
      const LocalSurfaceId& local_surface_id,
      const CompositorFrame& frame,
      const gfx::Rect& damage_rect,
      base::TimeTicks expected_display_time) override {}
  void OnSurfacePresented(uint32_t frame_token,
                          base::TimeTicks draw_start_timestamp,
                          const gfx::SwapTimings& swap_timings,
                          const gfx::PresentationFeedback& feedback) override {}
  bool IsVideoCaptureStarted() override;
  base::flat_set<base::PlatformThreadId> GetThreadIds() override;

  base::WeakPtrFactory<StubSurfaceClient> weak_factory{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_STUB_SURFACE_CLIENT_H_
