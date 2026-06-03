// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_observer.h"

#include "ui/latency/latency_info.h"

namespace viz {

bool SurfaceObserver::OnSurfaceDamaged(
    const SurfaceId& surface_id,
    const BeginFrameAck& ack,
    HandleInteraction handle_interaction,
    const std::vector<ui::LatencyInfo>& latency_info) {
  return false;
}

}  // namespace viz
