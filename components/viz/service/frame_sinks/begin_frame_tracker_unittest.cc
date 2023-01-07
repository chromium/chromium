// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/begin_frame_tracker.h"

#include <queue>

#include "base/containers/queue.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class BeginFrameTrackerTest : public testing::Test {
 public:
  void SendNextBeginFrame() {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE,
                                                         0, sequence_number_++);
    pending_acks_.push(BeginFrameAck(args, true));
    tracker_.SentBeginFrame(args);
  }

  void SendAck() {
    tracker_.ReceivedAck(pending_acks_.front());
    pending_acks_.pop();
  }

  void DropAck() { pending_acks_.pop(); }

 protected:
  BeginFrameTracker tracker_;
  uint64_t sequence_number_ = 1;
  std::queue<BeginFrameAck> pending_acks_;
};

// Verify that BeginFrameTracker throttles and unthrottles correctly.
TEST_F(BeginFrameTrackerTest, Throttle) {
  for (int i = 0; i < BeginFrameTracker::kLimitThrottle; ++i) {
    EXPECT_FALSE(tracker_.ShouldThrottleBeginFrame());
    EXPECT_FALSE(tracker_.ShouldStopBeginFrame());
    SendNextBeginFrame();
  }

  EXPECT_TRUE(tracker_.ShouldThrottleBeginFrame());
  EXPECT_FALSE(tracker_.ShouldStopBeginFrame());

  SendAck();

  EXPECT_FALSE(tracker_.ShouldThrottleBeginFrame());
  EXPECT_FALSE(tracker_.ShouldStopBeginFrame());
}

// Verify that BeginFrameTracker stops sending begin frames after kLimitStop.
TEST_F(BeginFrameTrackerTest, Stop) {
  for (int i = 0; i < BeginFrameTracker::kLimitStop; ++i) {
    EXPECT_FALSE(tracker_.ShouldStopBeginFrame());
    SendNextBeginFrame();
  }

  EXPECT_FALSE(tracker_.ShouldThrottleBeginFrame());
  EXPECT_TRUE(tracker_.ShouldStopBeginFrame());

  SendAck();

  EXPECT_TRUE(tracker_.ShouldThrottleBeginFrame());
  EXPECT_FALSE(tracker_.ShouldStopBeginFrame());
}

// Verify that BeginFrameTracker doesn't throttle a client that only acks half
// the time, as long as they ack the latest BeginFrameArgs.
TEST_F(BeginFrameTrackerTest, AllowDroppedAcks) {
  for (int i = 0; i < BeginFrameTracker::kLimitThrottle * 4; ++i) {
    EXPECT_FALSE(tracker_.ShouldThrottleBeginFrame());
    EXPECT_FALSE(tracker_.ShouldStopBeginFrame());
    SendNextBeginFrame();

    if (i % 2)
      SendAck();
    else
      DropAck();
  }
}

}  // namespace
}  // namespace viz
