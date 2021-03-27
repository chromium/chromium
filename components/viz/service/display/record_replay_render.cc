// Copyright 2020 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/record_replay_render.h"

#include "base/record_replay.h"

namespace viz {

void RecordReplaySubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                                       const viz::CompositorFrame& frame) {
  recordreplay::Print("RecordReplaySubmitCompositorFrame");
}

} // namespace viz
