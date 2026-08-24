// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/metrics_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {
namespace {

using ::testing::Test;

class SendTabToSelfMetricsUtilTest : public Test {
 protected:
  base::HistogramTester histogram_tester_;
};

// Verifies that RecordNotificationStatus records all notification status
// variants to the Sharing.SendTabToSelf.NotificationStatus histogram.
TEST_F(SendTabToSelfMetricsUtilTest, RecordNotificationStatusAllValues) {
  // Test each valid status individually to ensure correct enum mapping.
  RecordNotificationStatus(NotificationStatus::kShown);
  histogram_tester_.ExpectBucketCount(
      "Sharing.SendTabToSelf.NotificationStatus", NotificationStatus::kShown,
      1);

  RecordNotificationStatus(NotificationStatus::kDismissed);
  histogram_tester_.ExpectBucketCount(
      "Sharing.SendTabToSelf.NotificationStatus",
      NotificationStatus::kDismissed, 1);

  RecordNotificationStatus(NotificationStatus::kOpened);
  histogram_tester_.ExpectBucketCount(
      "Sharing.SendTabToSelf.NotificationStatus", NotificationStatus::kOpened,
      1);

  RecordNotificationStatus(NotificationStatus::kTimedOut);
  histogram_tester_.ExpectBucketCount(
      "Sharing.SendTabToSelf.NotificationStatus", NotificationStatus::kTimedOut,
      1);

  RecordNotificationStatus(NotificationStatus::kThrottled);
  histogram_tester_.ExpectBucketCount(
      "Sharing.SendTabToSelf.NotificationStatus",
      NotificationStatus::kThrottled, 1);

  histogram_tester_.ExpectTotalCount("Sharing.SendTabToSelf.NotificationStatus",
                                     5);
}

}  // namespace
}  // namespace send_tab_to_self
