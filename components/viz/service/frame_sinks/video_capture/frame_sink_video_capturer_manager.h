// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_MANAGER_H_

namespace viz {

class CapturableFrameSink;
struct VideoCaptureTarget;
class FrameSinkVideoCapturerImpl;

// Interface implemented by the owner/manager of FrameSinkVideoCapturerImpl
// instances. This allows the capturer to query for the existence of a frame
// sink target and also notify the owner when it should be destroyed.
class FrameSinkVideoCapturerManager {
 public:
  // Returns the CapturableFrameSink implementation associated with the given
  // |target|, or nullptr if unknown.
  virtual CapturableFrameSink* FindCapturableFrameSink(
      const VideoCaptureTarget& target) = 0;

  // Called once, when the mojo binding for the given |capturer| has been
  // closed. At this point, the capturer is a zombie waiting to be destroyed.
  virtual void OnCapturerConnectionLost(
      FrameSinkVideoCapturerImpl* capturer) = 0;

 protected:
  virtual ~FrameSinkVideoCapturerManager() = default;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_MANAGER_H_
