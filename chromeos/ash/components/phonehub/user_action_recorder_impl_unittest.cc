// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/user_action_recorder_impl.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {
namespace {
const char kCompletedActionMetricName[] = "PhoneHub.CompletedUserAction";
}  // namespace

class UserActionRecorderImplTest : public testing::Test {
 protected:
  UserActionRecorderImplTest()
      : fake_feature_status_provider_(FeatureStatus::kEnabledAndConnected),
        recorder_(&fake_feature_status_provider_) {}
  UserActionRecorderImplTest(const UserActionRecorderImplTest&) = delete;
  UserActionRecorderImplTest& operator=(const UserActionRecorderImplTest&) =
      delete;
  ~UserActionRecorderImplTest() override = default;

  FakeFeatureStatusProvider fake_feature_status_provider_;
  UserActionRecorderImpl recorder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(UserActionRecorderImplTest, RecordActions) {
  recorder_.RecordUiOpened();
  recorder_.RecordTetherConnectionAttempt();
  recorder_.RecordDndAttempt();
  recorder_.RecordFindMyDeviceAttempt();
  recorder_.RecordBrowserTabOpened();
  recorder_.RecordNotificationDismissAttempt();
  recorder_.RecordNotificationReplyAttempt();
  recorder_.RecordCameraRollDownloadAttempt();

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
  histogram_tester_.ExpectBucketCount(
      kCompletedActionMetricName,
      UserActionRecorderImpl::UserAction::kCameraRollDownload,
      /*expected_count=*/1);
}

}  // namespace phonehub
}  // namespace ash
