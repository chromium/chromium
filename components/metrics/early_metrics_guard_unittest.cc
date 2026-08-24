// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/early_metrics_guard.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(EarlyMetricsGuardTest, FlagState) {
  EXPECT_FALSE(EarlyMetricsGuard::IsEarlyMetricsRecordingActive());

  {
    EarlyMetricsGuard guard;
    EXPECT_TRUE(EarlyMetricsGuard::IsEarlyMetricsRecordingActive());
  }

  EXPECT_FALSE(EarlyMetricsGuard::IsEarlyMetricsRecordingActive());
}

TEST(EarlyMetricsGuardTest, NestedGuardsDcheck) {
  EarlyMetricsGuard guard1;
  EXPECT_DCHECK_DEATH({ EarlyMetricsGuard guard2; });
}

}  // namespace metrics
