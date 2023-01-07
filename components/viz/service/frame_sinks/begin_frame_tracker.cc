// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/begin_frame_tracker.h"

namespace viz {

void BeginFrameTracker::SentBeginFrame(const BeginFrameArgs& args) {
  ++outstanding_begin_frames_;
  last_frame_id_ = args.frame_id;
}

void BeginFrameTracker::ReceivedAck(const BeginFrameAck& ack) {
  if (MatchesLastSent(ack)) {
    // If the BeginFrameAck matches the last sent BeginFrameArgs then we know
    // the client has read every message from the pipe. While the client
    // should send an ack for every args, this guards against bugs that make a
    // responsive client occasional drop a begin frame with no ack. Otherwise
    // the occasional dropped ack would add up and eventually throttle then
    // stop sending begin frames entirely.
    outstanding_begin_frames_ = 0;
  } else if (outstanding_begin_frames_ > 0) {
    // The condition above makes it such that we aren't necessarily evenly
    // incrementing/decrementing |outstanding_begin_frames_|, so ensure it
    // never goes negative.
    --outstanding_begin_frames_;
  }
}

bool BeginFrameTracker::ShouldThrottleBeginFrame() const {
  return outstanding_begin_frames_ >= kLimitThrottle &&
         outstanding_begin_frames_ < kLimitStop;
}

bool BeginFrameTracker::ShouldStopBeginFrame() const {
  return outstanding_begin_frames_ >= kLimitStop;
}

bool BeginFrameTracker::MatchesLastSent(const BeginFrameAck& ack) {
  return last_frame_id_ == ack.frame_id;
}

}  // namespace viz
