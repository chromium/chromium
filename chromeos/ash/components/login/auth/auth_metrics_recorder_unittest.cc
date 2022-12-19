// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AuthMetricsRecorderTest : public ::testing::Test {
 public:
  AuthMetricsRecorderTest() {
    recorder_ = AuthMetricsRecorder::CreateForTesting();
  }

  ~AuthMetricsRecorderTest() override { recorder_.reset(); }

 protected:
  std::unique_ptr<ash::AuthMetricsRecorder> recorder_;
};

TEST_F(AuthMetricsRecorderTest, OnAuthFailure) {
  base::HistogramTester histogram_tester;

  recorder_->OnAuthFailure(
      AuthFailure::FailureReason::COULD_NOT_MOUNT_CRYPTOHOME);
  histogram_tester.ExpectTotalCount("Login.FailureReason", 1);
  histogram_tester.ExpectBucketCount(
      "Login.FailureReason",
      (int)AuthFailure::FailureReason::COULD_NOT_MOUNT_CRYPTOHOME, 1);
}

TEST_F(AuthMetricsRecorderTest, OnLoginSuccess) {
  base::HistogramTester histogram_tester;

  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_ONLY);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 1);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason", static_cast<int>(SuccessReason::OFFLINE_ONLY), 1);
}

TEST_F(AuthMetricsRecorderTest, LoginFlowShowUsers) {
  base::HistogramTester histogram_tester;

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/true);
  recorder_->OnEnableEphemeralUsers(/*enable_ephemeral_users=*/false);
  recorder_->OnUserCount(/*user_count=*/1);
  recorder_->OnIsUserNew(/*is_new_user=*/false);
  recorder_->OnIsLoginOffline(/*is_login_offline=*/true);
  histogram_tester.ExpectTotalCount("Login.Flow.ShowUsers.1", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.ShowUsers.1", static_cast<int>(AuthMetricsRecorder::kOffline),
      1);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/true);
  recorder_->OnEnableEphemeralUsers(/*enable_ephemeral_users=*/false);
  recorder_->OnUserCount(/*user_count=*/3);
  recorder_->OnIsUserNew(/*is_new_user=*/false);
  recorder_->OnIsLoginOffline(/*is_login_offline=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.ShowUsers.Few", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.ShowUsers.Few",
      static_cast<int>(AuthMetricsRecorder::kOnlineExisting), 1);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/true);
  recorder_->OnEnableEphemeralUsers(/*enable_ephemeral_users=*/false);
  recorder_->OnUserCount(/*user_count=*/33);
  recorder_->OnIsUserNew(/*is_new_user=*/true);
  recorder_->OnIsLoginOffline(/*is_login_offline=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.ShowUsers.Many", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.ShowUsers.Many",
      static_cast<int>(AuthMetricsRecorder::kOnlineNew), 1);
}

TEST_F(AuthMetricsRecorderTest, LoginFlowHideUsers) {
  base::HistogramTester histogram_tester;

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/false);
  recorder_->OnEnableEphemeralUsers(/*enable_ephemeral_users=*/false);
  recorder_->OnUserCount(/*user_count=*/1);
  recorder_->OnIsUserNew(/*is_new_user=*/false);
  recorder_->OnIsLoginOffline(/*is_login_offline=*/true);
  histogram_tester.ExpectTotalCount("Login.Flow.HideUsers.1", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.HideUsers.1", static_cast<int>(AuthMetricsRecorder::kOffline),
      1);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/false);
  recorder_->OnEnableEphemeralUsers(/*enable_ephemeral_users=*/false);
  recorder_->OnUserCount(/*user_count=*/3);
  recorder_->OnIsUserNew(/*is_new_user=*/false);
  recorder_->OnIsLoginOffline(/*is_login_offline=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.HideUsers.Few", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.HideUsers.Few",
      static_cast<int>(AuthMetricsRecorder::kOnlineExisting), 1);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/false);
  recorder_->OnEnableEphemeralUsers(/*enable_ephemeral_users=*/false);
  recorder_->OnUserCount(/*user_count=*/33);
  recorder_->OnIsUserNew(/*is_new_user=*/true);
  recorder_->OnIsLoginOffline(/*is_login_offline=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.HideUsers.Many", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.HideUsers.Many",
      static_cast<int>(AuthMetricsRecorder::kOnlineNew), 1);
}

}  // namespace ash
