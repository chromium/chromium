// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/user_action_recorder_impl.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {
const char kCompletedActionMetricName[] = "PhoneHub.CompletedUserAction";
}  // namespace

class UserActionRecorderImplTest : public testing::Test {
 protected:
  UserActionRecorderImplTest() = default;
  UserActionRecorderImplTest(const UserActionRecorderImplTest&) = delete;
  UserActionRecorderImplTest& operator=(const UserActionRecorderImplTest&) =
      delete;
  ~UserActionRecorderImplTest() override = default;

  UserActionRecorderImpl recorder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(UserActionRecorderImplTest, Enabled_RecordActions) {
  recorder_.RecordUiOpened();
  recorder_.RecordTetherConnectionAttempt();
  recorder_.RecordDndAttempt();
  recorder_.RecordFindMyDeviceAttempt();
  recorder_.RecordBrowserTabOpened();
  recorder_.RecordNotificationDismissAttempt();
  recorder_.RecordNotificationReplyAttempt();

  // Each of the actions should have been completed
  histogram_tester_.ExpectBucketCount(
      kCompletedActionMetricName, UserActionRecorderImpl::UserAction::kUiOpened,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      kCompletedActionMetricName, UserActionRecorderImpl::UserAction::kTether,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kCompletedActionMetricName,
                                      UserActionRecorderImpl::UserAction::kDnd,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      kCompletedActionMetricName,
      UserActionRecorderImpl::UserAction::kFindMyDevice,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      kCompletedActionMetricName,
      UserActionRecorderImpl::UserAction::kBrowserTab,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      kCompletedActionMetricName,
      UserActionRecorderImpl::UserAction::kNotificationDismissal,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      kCompletedActionMetricName,
      UserActionRecorderImpl::UserAction::kNotificationReply,
      /*expected_count=*/1);
}

}  // namespace phonehub
}  // namespace chromeos
