// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/timer_sampling_event_source.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_metrics {

TEST(TimerSamplingEventSourceTest, Basic) {
  constexpr base::TimeDelta kDelay = base::Seconds(1);
  int num_callbacks = 0;
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  TimerSamplingEventSource source(kDelay);
  EXPECT_TRUE(
      source.Start(base::BindLambdaForTesting([&]() { ++num_callbacks; })));
  EXPECT_EQ(0, num_callbacks);
  task_environment.FastForwardBy(kDelay / 2);
  EXPECT_EQ(0, num_callbacks);
  task_environment.FastForwardBy(kDelay / 2);
  EXPECT_EQ(1, num_callbacks);
  task_environment.FastForwardBy(kDelay * 10);
  EXPECT_EQ(11, num_callbacks);
}

}  // namespace power_metrics
