// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_

#include <memory>

#include "base/time/time.h"
#include "components/viz/service/surfaces/pending_copy_output_request.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace viz {

class CompositorFrameMetadata;
class LocalSurfaceId;

// Interface for CompositorFrameSink implementations that support frame sink
// video capture.
class CapturableFrameSink {
 public:
  // Interface for a client that observes certain frame events and calls
  // RequestCopyOfSurface() at the appropriate times.
  class Client {
   public:
    virtual ~Client() = default;

    // Called when a frame's content, or that of one or more of its child
    // frames, has changed. |frame_size| is the output size of the currently-
    // active compositor frame for the frame sink being monitored, with
    // |damage_rect| being the region within that has changed (never empty).
    // |expected_display_time| indicates when the content change was expected to
    // appear on the Display.
    virtual void OnFrameDamaged(
        const gfx::Size& frame_size,
        const gfx::Rect& damage_rect,
        base::TimeTicks expected_display_time,
        const CompositorFrameMetadata& frame_metadata) = 0;
  };

  virtual ~CapturableFrameSink() = default;

  // Attach/Detach a video capture client to the frame sink. The client will
  // receive frame begin and draw events, and issue copy requests, when
  // appropriate.
  virtual void AttachCaptureClient(Client* client) = 0;
  virtual void DetachCaptureClient(Client* client) = 0;

  // Returns the currently-active frame size, or an empty size if there is no
  // active frame.
  virtual gfx::Size GetActiveFrameSize() = 0;

  // Issues a request for a copy of the next composited frame whose
  // LocalSurfaceId is at least |local_surface_id|. Note that if this id is
  // default constructed, then the next surface will provide the copy output
  // regardless of its LocalSurfaceId.
  virtual void RequestCopyOfOutput(
      PendingCopyOutputRequest pending_copy_output_request) = 0;

  // Returns the CompositorFrameMetadata of the last activated CompositorFrame.
  // Return null if no CompositorFrame has activated yet.
  virtual const CompositorFrameMetadata* GetLastActivatedFrameMetadata() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_
