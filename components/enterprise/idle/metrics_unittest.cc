// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/metrics.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_idle {

// Tests that IdleTimeoutCase enum values remain unchanged.
TEST(IdleTimeoutMetricsEnumsTest, IdleTimeoutCase) {
  EXPECT_EQ(static_cast<int>(metrics::IdleTimeoutCase::kForeground), 0);
  EXPECT_EQ(static_cast<int>(metrics::IdleTimeoutCase::kBackground), 1);
  // Update `kMaxValue` if a new enum value is added.
  EXPECT_EQ(metrics::IdleTimeoutCase::kMaxValue,
            metrics::IdleTimeoutCase::kBackground);
}

// Tests that IdleTimeoutDialogEvent enum values remain unchanged.
TEST(IdleTimeoutMetricsEnumsTest, IdleTimeoutDialogEvent) {
  EXPECT_EQ(static_cast<int>(metrics::IdleTimeoutDialogEvent::kDialogShown), 0);
  EXPECT_EQ(
      static_cast<int>(metrics::IdleTimeoutDialogEvent::kDialogDismissedByUser),
      1);
  EXPECT_EQ(static_cast<int>(metrics::IdleTimeoutDialogEvent::kDialogExpired),
            2);
  // Update `kMaxValue` if a new enum value is added.
  EXPECT_EQ(metrics::IdleTimeoutDialogEvent::kMaxValue,
            metrics::IdleTimeoutDialogEvent::kDialogExpired);
}

// Tests that IdleTimeoutLaunchScreenEvent enum values remain unchanged.
TEST(IdleTimeoutMetricsEnumsTest, IdleTimeoutLaunchScreenEvent) {
  EXPECT_EQ(static_cast<int>(
                metrics::IdleTimeoutLaunchScreenEvent::kLaunchScreenShown),
            0);
  EXPECT_EQ(static_cast<int>(metrics::IdleTimeoutLaunchScreenEvent::
                                 kLaunchScreenDismissedAfterActionCompletion),
            1);
  EXPECT_EQ(static_cast<int>(
                metrics::IdleTimeoutLaunchScreenEvent::kLaunchScreenExpired),
            2);
  // Update `kMaxValue` if a new enum value is added.
  EXPECT_EQ(metrics::IdleTimeoutLaunchScreenEvent::kMaxValue,
            metrics::IdleTimeoutLaunchScreenEvent::kLaunchScreenExpired);
}

}  // namespace enterprise_idle
