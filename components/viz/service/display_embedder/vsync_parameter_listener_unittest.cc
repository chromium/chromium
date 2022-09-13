// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/vsync_parameter_listener.h"

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class VSyncParameterListenerTestRunner {
 public:
  VSyncParameterListenerTestRunner(int64_t interval_us, int64_t timebase_us)
      : interval_(base::Microseconds(interval_us)),
        timebase_(base::TimeTicks() + base::Microseconds(timebase_us)) {}

  void RunTests() {
    const uint64_t interval_us = interval_.InMicroseconds();
    const uint64_t half_inteval_us = interval_us / 2;
    const uint64_t max_timebase_skew_us =
        VSyncParameterListener::kMaxTimebaseSkew.InMicroseconds();

    // Timebase values very near the last timebase. Don't send an update.
    EXPECT_FALSE(WillSendUpdate(max_timebase_skew_us - 1));
    EXPECT_FALSE(WillSendUpdate(-1));
    EXPECT_FALSE(WillSendUpdate(0));
    EXPECT_FALSE(WillSendUpdate(1));
    EXPECT_FALSE(WillSendUpdate(max_timebase_skew_us - 1));

    // Timebase values in between intervals. Send an update.
    EXPECT_TRUE(WillSendUpdate(max_timebase_skew_us));
    EXPECT_TRUE(WillSendUpdate(half_inteval_us - 10));
    EXPECT_TRUE(WillSendUpdate(half_inteval_us));
    EXPECT_TRUE(WillSendUpdate(half_inteval_us + 10));
    EXPECT_TRUE(WillSendUpdate(interval_us - max_timebase_skew_us));

    // Timebase is near last timebase + interval. Don't send an update.
    EXPECT_FALSE(WillSendUpdate(interval_us - max_timebase_skew_us + 1));
    EXPECT_FALSE(WillSendUpdate(interval_us - 1));
    EXPECT_FALSE(WillSendUpdate(interval_us));
    EXPECT_FALSE(WillSendUpdate(interval_us + 1));
    EXPECT_FALSE(WillSendUpdate(interval_us + max_timebase_skew_us - 1));

    // Timebase values in between intervals but further away. Send an update.
    EXPECT_TRUE(WillSendUpdate(interval_us + max_timebase_skew_us));
    EXPECT_TRUE(WillSendUpdate(interval_us + half_inteval_us));
    EXPECT_TRUE(WillSendUpdate(2 * interval_us - max_timebase_skew_us));
    EXPECT_TRUE(WillSendUpdate(2 * interval_us + max_timebase_skew_us));
  }

 private:
  // Checks if VSyncParameterListener will send an update when it sees
  // |timebase_| and then |timebase_| + |timebase_difference_us|.
  bool WillSendUpdate(int64_t timebase_difference_us) {
    VSyncParameterListener listener{/*observer=*/mojo::NullRemote()};
    EXPECT_TRUE(listener.ShouldSendUpdate(timebase_, interval_));

    return listener.ShouldSendUpdate(
        timebase_ + base::Microseconds(timebase_difference_us), interval_);
  }

  base::TimeDelta interval_;
  base::TimeTicks timebase_;
};

// Verify that timebase skew calculation works correctly near multiples of
// the interval.
TEST(VSyncParameterListenerTest, TimebaseMultipleOfInterval) {
  uint64_t interval = 16671;
  uint64_t timebase = interval * 100;
  VSyncParameterListenerTestRunner(interval, timebase).RunTests();
}

// Verify that timebase skew calculation works correctly when timebase isn't
// anywhere near a multiple of the interval.
TEST(VSyncParameterListenerTest, TimebaseNotNearInterval) {
  uint64_t interval = 16671;
  uint64_t timebase = (interval * 100) + (interval / 2);
  VSyncParameterListenerTestRunner(interval, timebase).RunTests();
}

}  // namespace viz
