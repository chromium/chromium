// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_throttler.h"

#include <algorithm>

#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class FrameSinkThrottlerTest : public testing::Test {
 public:
  FrameSinkThrottlerTest() = default;
  ~FrameSinkThrottlerTest() override = default;

 protected:
  FrameSinkThrottler throttler_;
};

TEST_F(FrameSinkThrottlerTest, DefaultState) {
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
  EXPECT_FALSE(throttler_.IsThrottledBySimpleCadence());
}

TEST_F(FrameSinkThrottlerTest, SetThrottleInterval) {
  base::TimeDelta interval = base::Hertz(30);
  throttler_.SetThrottleInterval(interval);
  EXPECT_EQ(throttler_.begin_frame_interval(), interval);
  EXPECT_FALSE(throttler_.IsThrottledBySimpleCadence());

  throttler_.SetThrottleInterval(base::TimeDelta());
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
}

TEST_F(FrameSinkThrottlerTest, SetCadenceThrottleInterval) {
  base::TimeDelta interval = base::Hertz(30);
  throttler_.SetCadenceThrottleInterval(interval);
  // Cadence throttling should activate without knowing vsync interval
  EXPECT_EQ(throttler_.begin_frame_interval(), interval);
  EXPECT_TRUE(throttler_.IsThrottledBySimpleCadence());

  // Set a compatible vsync interval (60Hz)
  throttler_.SetLastKnownVsync(base::Hertz(60), base::Hertz(60));
  EXPECT_EQ(throttler_.begin_frame_interval(), interval);
  EXPECT_TRUE(throttler_.IsThrottledBySimpleCadence());

  // Set an incompatible vsync interval (60Hz vs 24Hz)
  throttler_.SetCadenceThrottleInterval(base::Hertz(24));
  throttler_.SetLastKnownVsync(base::Hertz(60), base::Hertz(60));
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
  EXPECT_FALSE(throttler_.IsThrottledBySimpleCadence());
}

TEST_F(FrameSinkThrottlerTest, ThrottleIntervalPriority) {
  base::TimeDelta throttle_interval = base::Hertz(20);
  base::TimeDelta cadence_interval = base::Hertz(30);
  base::TimeDelta vsync_interval = base::Hertz(60);

  throttler_.SetLastKnownVsync(vsync_interval, vsync_interval);
  throttler_.SetCadenceThrottleInterval(cadence_interval);
  throttler_.SetThrottleInterval(throttle_interval);

  // Explicit throttle (20Hz) is slower than cadence (30Hz), but cadence wins.
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);
  EXPECT_TRUE(throttler_.IsThrottledBySimpleCadence());

  // Background interval (> 1s) is also slower, but cadence wins.
  base::TimeDelta background_interval = base::Seconds(2);
  throttler_.SetThrottleInterval(background_interval);
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);
}

TEST_F(FrameSinkThrottlerTest, UnknownVsync) {
  base::TimeDelta interval = base::Hertz(30);
  throttler_.SetCadenceThrottleInterval(interval);

  // No vsync known, so IsThrottledBySimpleCadence() returns true.
  EXPECT_TRUE(throttler_.IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), interval);
}

TEST_F(FrameSinkThrottlerTest, ScreenCaptureUnthrottlesVideoCadence) {
  base::TimeDelta cadence_interval = base::Hertz(30);
  throttler_.SetCadenceThrottleInterval(cadence_interval);

  // Simulate screen capture start: allow_throttling = false.
  throttler_.SetAllowThrottling(false);

  // Should be unthrottled (0), overriding cadence.
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
  EXPECT_FALSE(throttler_.throttling_allowed());

  // Simulate screen capture stop: allow_throttling = true.
  throttler_.SetAllowThrottling(true);
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);
  EXPECT_TRUE(throttler_.throttling_allowed());
}

}  // namespace viz
