// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PERF_TEST_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PERF_TEST_H_

#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

// Viz perf test base class that sets up a lap timer with a specified duration.
class VizPerfTest : public testing::Test {
 public:
  VizPerfTest();

 protected:
  // Duration is set by the flag --perf-test-time-ms, defaults to 3 seconds.
  base::LapTimer timer_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PERF_TEST_H_
