// Copyright 2020 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RECORD_REPLAY_RENDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RECORD_REPLAY_RENDER_H_

#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace viz {

// When recording, renderer processes generate compositor frames in the usual
// way and send them on to the GPU process for drawing to the screen. When
// replaying (and optionally when recording, for debugging) the process
// additionally sends these frames to an in process renderer for updating an
// in process buffer with the data the process is currently drawing. This data
// can then be encoded to base64 images and reported to the record/replay
// driver and sent to clients inspecting the recording.

void RecordReplaySubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                                       const viz::CompositorFrame& frame);

} // namespace viz

#endif // COMPONENTS_VIZ_SERVICE_DISPLAY_RECORD_REPLAY_RENDER_H_
