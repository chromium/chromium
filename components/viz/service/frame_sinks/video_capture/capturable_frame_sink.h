// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_

#include "base/time/time.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/service/surfaces/pending_copy_output_request.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

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
    // frames, has changed. `root_render_pass_size` is the output size of the
    // currently-active compositor frame for the frame sink being monitored,
    // with `damage_rect` being the region within that has changed (never
    // empty). `expected_display_time` indicates when the content change was
    // expected to appear on the Display.
    virtual void OnFrameDamaged(
        const gfx::Size& root_render_pass_size,
        const gfx::Rect& damage_rect,
        base::TimeTicks expected_display_time,
        const CompositorFrameMetadata& frame_metadata) = 0;

    // Called from SurfaceAggregator to get the video capture status on the
    // surface which is going to be drawn to.
    virtual bool IsVideoCaptureStarted() = 0;
  };

  virtual ~CapturableFrameSink() = default;

  virtual const FrameSinkId& GetFrameSinkId() const = 0;

  // Attach/Detach a video capture client to the frame sink. The client will
  // receive frame begin and draw events, and issue copy requests, when
  // appropriate.
  virtual void AttachCaptureClient(Client* client) = 0;
  virtual void DetachCaptureClient(Client* client) = 0;

  // Properties of the region associated with the video capture sub target that
  // has been selected for capture.
  struct RegionProperties {
    // The size of the root render pass, in its own coordinate space's physical
    // pixels.
    gfx::Size root_render_pass_size;

    // The area inside of the render pass that should be captured, in its own
    // coordinate space in physical pixels. May be the entire render pass output
    // rect, and may or may not be the root render pass.
    gfx::Rect render_pass_subrect;

    // The transform from the render pass coordinate space to the root render
    // pass coordinate space. When applied to `render_pass_subrect`, performs a
    // transformation of each coordinate into the corresponding coordinate in
    // `root_render_pass_size`.
    gfx::Transform transform_to_root;
  };

  // Returns the capture region information associated with the given
  // `sub_target`, which includes the subsection of the render pass that should
  // be capture, as well as information about the root render pass. Will return
  // std::nullopt if either of the render pass subrect or the compositor frame
  // size are empty.
  virtual std::optional<RegionProperties> GetRequestRegionProperties(
      const VideoCaptureSubTarget& sub_target) const = 0;

  // Called when a video capture client starts or stops capturing.
  virtual void OnClientCaptureStarted() = 0;
  virtual void OnClientCaptureStopped() = 0;

  // Issues a request for a copy of the next composited frame whose
  // LocalSurfaceId is at least `local_surface_id`. Note that if this id is
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
