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

  bool IsThrottledBySimpleCadence() {
    return throttler_.IsThrottledBySimpleCadence();
  }

 protected:
  FrameSinkThrottler throttler_;
};

TEST_F(FrameSinkThrottlerTest, DefaultStateAllowsThrottling) {
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
  EXPECT_FALSE(IsThrottledBySimpleCadence());
  EXPECT_TRUE(throttler_.throttling_allowed());
}

TEST_F(FrameSinkThrottlerTest, BasicThrottleInterval) {
  base::TimeDelta interval = base::Hertz(30);
  throttler_.SetThrottleInterval(interval);
  EXPECT_EQ(throttler_.begin_frame_interval(), interval);
  EXPECT_FALSE(IsThrottledBySimpleCadence());

  throttler_.SetThrottleInterval(base::TimeDelta());
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
}

// SetAllowThrottling(false) never sets begin_frame_interval().
TEST_F(FrameSinkThrottlerTest, ThrottlingNotAllowed) {
  throttler_.SetAllowThrottling(false);

  // Attempting to set various cadences should not reset begin_frame_interval().
  // Basic throttling
  base::TimeDelta interval = base::Hertz(24);
  throttler_.SetThrottleInterval(interval);
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());

  // Cadence throttling
  base::TimeDelta cadence_interval = base::Hertz(30);
  throttler_.SetCadenceThrottleInterval(cadence_interval);
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());

  // Cadence throttling with compatible known vsync interval
  throttler_.SetLastKnownVsync(base::Hertz(60), base::Hertz(60));
  EXPECT_TRUE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());

  // Cadence throttling with incompatible known vsync interval
  throttler_.SetLastKnownVsync(base::Hertz(144), base::Hertz(144));
  EXPECT_FALSE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());

  // Interaction throttling
  throttler_.SetThrottledDueToInteraction(true);
  EXPECT_FALSE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
}

// If a cadence interval is set, throttling to said interval
// should be possible if the cadence interval is a perfect
// divisor of the last known vsync interval.
TEST_F(FrameSinkThrottlerTest, SetCadenceThrottleInterval) {
  base::TimeDelta cadence_interval = base::Hertz(30);
  throttler_.SetCadenceThrottleInterval(cadence_interval);
  // Cadence throttling should activate without knowing vsync interval
  EXPECT_TRUE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);

  // Set a compatible vsync interval (60Hz) triggers throttling.
  throttler_.SetLastKnownVsync(base::Hertz(60), base::Hertz(60));
  EXPECT_TRUE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);

  // Set an incompatible vsync interval (60Hz vs 24Hz) prevents throttling.
  throttler_.SetCadenceThrottleInterval(base::Hertz(24));
  EXPECT_FALSE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), base::TimeDelta());
}

TEST_F(FrameSinkThrottlerTest, ThrottleVsCadenceIntervalPriority) {
  base::TimeDelta throttle_interval = base::Hertz(20);
  base::TimeDelta cadence_interval = base::Hertz(30);
  base::TimeDelta vsync_interval = base::Hertz(60);

  throttler_.SetLastKnownVsync(vsync_interval, vsync_interval);
  throttler_.SetCadenceThrottleInterval(cadence_interval);
  throttler_.SetThrottleInterval(throttle_interval);

  // Cadence throttle takes priority over explicit throttling
  // because it is an even divisor of the vsync interval.
  EXPECT_TRUE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);

  // Changing the last known vsync interval to not be a multiple of the
  // cadence interval causes the begin_frame_interval() to fall back
  // on the explicit throttle interval.
  throttler_.SetLastKnownVsync(base::Hertz(144), base::Hertz(144));
  EXPECT_FALSE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), throttle_interval);
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

// SetThrottledDueToInteraction interval changes with to known vsync
TEST_F(FrameSinkThrottlerTest, ThrottleInteractionsBasic) {
  // Set throttle on interactions to true causes the throttling cadence
  // to be half the dfault framerate.
  throttler_.SetThrottledDueToInteraction(true);
  EXPECT_EQ(throttler_.begin_frame_interval(),
            BeginFrameArgs::DefaultInterval() * 2);

  // Vsync interval is used if it is known.
  throttler_.SetLastKnownVsync(base::Hertz(144), base::Hertz(144));
  EXPECT_EQ(throttler_.begin_frame_interval(), base::Hertz(144) * 2);

  // Setting an explicit smaller throttle interval does not change behaviour.
  throttler_.SetThrottleInterval(base::Hertz(240));
  EXPECT_EQ(throttler_.begin_frame_interval(), base::Hertz(144) * 2);

  // Setting an explicit larger throttle interval overrides the behaviour of the
  // throttle
  throttler_.SetThrottleInterval(base::Hertz(20));
  EXPECT_EQ(throttler_.begin_frame_interval(), base::Hertz(20));
}

// SetThrottledDueToInteraction interval ignored when simple cadence is set.
TEST_F(FrameSinkThrottlerTest,
       CadenceIntervalPriorityOverInteractionThrottling) {
  base::TimeDelta cadence_interval = base::Hertz(30);
  base::TimeDelta very_fast_interval = base::Hertz(240);
  base::TimeDelta very_slow_interval = base::Hertz(20);
  throttler_.SetThrottledDueToInteraction(true);
  throttler_.SetCadenceThrottleInterval(cadence_interval);

  // Cadence interval takes precedence over interaction throttle behaviour.
  EXPECT_TRUE(IsThrottledBySimpleCadence());
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);

  // Setting an explicit smaller throttle interval does not change behaviour.
  throttler_.SetThrottleInterval(very_fast_interval);
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);

  // Setting an explicit larger throttle interval does not change behaviour.
  throttler_.SetThrottleInterval(very_slow_interval);
  EXPECT_EQ(throttler_.begin_frame_interval(), cadence_interval);

  // Disabling cadence by setting an incompatible last known vsync causes
  // fallback to last known explicit throttle interval.
  throttler_.SetLastKnownVsync(base::Hertz(144), base::Hertz(144));
  EXPECT_EQ(throttler_.begin_frame_interval(), very_slow_interval);
}

}  // namespace viz
