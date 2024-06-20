// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/time_measurements.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TEST(TimeMeasurementsTest, ScopedUmaHistogramThreadTimer) {
  base::HistogramTester tester;
  {
    auto timer = ScopedUmaHistogramThreadTimer("ScopedTimers.ThreadTimer");

    tester.ExpectTotalCount("ScopedTimers.ThreadTimer", 0);
  }

  int expected_count = ScopedThreadTimers::IsSupported() ? 1 : 0;
  tester.ExpectTotalCount("ScopedTimers.ThreadTimer", expected_count);
}

TEST(TimeMeasurementsTest, ScopedUmaHistogramMicroThreadTimer) {
  base::HistogramTester tester;
  {
    auto timer =
        ScopedUmaHistogramMicroThreadTimer("ScopedTimers.MicroThreadTimer");

    tester.ExpectTotalCount("ScopedTimers.MicroThreadTimer", 0);
  }

  int expected_count = ScopedThreadTimers::IsSupported() ? 1 : 0;
  tester.ExpectTotalCount("ScopedTimers.MicroThreadTimer", expected_count);
}

}  // namespace subresource_filter
