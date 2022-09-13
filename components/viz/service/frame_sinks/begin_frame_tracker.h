// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_BEGIN_FRAME_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_BEGIN_FRAME_TRACKER_H_

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// Keeps track of OnBeginFrame() messages sent the client and acks received back
// from the client in order to throttle unresponsive clients. This is to prevent
// sending a large number of IPCs to a client that is unresponsive and having
// the message queue balloon in size. Tolerates clients occasionally dropping
// an OnBeginFrame() message and not acking as long as they keep responding to
// the latest BeginFrameArgs.
class VIZ_SERVICE_EXPORT BeginFrameTracker {
 public:
  // Defines the number of begin frames that have been sent to a client without
  // a response before we throttle or stop sending begin frames altogether.
  static constexpr int kLimitStop = 100;
  static constexpr int kLimitThrottle = 10;

  // To be called every time OnBeginFrame() is sent.
  void SentBeginFrame(const BeginFrameArgs& args);

  // To be called every time a BeginFrameAck is received back from the client.
  void ReceivedAck(const BeginFrameAck& ack);

  bool ShouldThrottleBeginFrame() const;
  bool ShouldStopBeginFrame() const;

 private:
  bool MatchesLastSent(const BeginFrameAck& ack);

  int outstanding_begin_frames_ = 0;

  BeginFrameId last_frame_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_BEGIN_FRAME_TRACKER_H_
