// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_events_recorder.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

std::string GetSessionStateCrashKeyValue() {
  return crash_reporter::GetCrashKeyValue("session-state");
}

}  // namespace

class AuthEventsRecorderTest : public ::testing::Test {
 public:
  AuthEventsRecorderTest() {
    crash_reporter::InitializeCrashKeysForTesting();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    recorder_ = AuthEventsRecorder::CreateForTesting();
  }

  ~AuthEventsRecorderTest() override { recorder_.reset(); }

 protected:
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ash::AuthEventsRecorder> recorder_;
};

TEST_F(AuthEventsRecorderTest, OnAuthFailure) {
  base::HistogramTester histogram_tester;

  recorder_->OnAuthFailure(
      AuthFailure::FailureReason::COULD_NOT_MOUNT_CRYPTOHOME);
  histogram_tester.ExpectTotalCount("Login.FailureReason", 1);
  histogram_tester.ExpectBucketCount(
      "Login.FailureReason",
      (int)AuthFailure::FailureReason::COULD_NOT_MOUNT_CRYPTOHOME, 1);
}

TEST_F(AuthEventsRecorderTest, LoginFlowShowUsers) {
  base::HistogramTester histogram_tester;

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/true);
  recorder_->OnUserCount(/*user_count=*/1);
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_AND_ONLINE,
                            /*is_new_user=*/false, /*is_login_offline=*/true,
                            /*is_ephemeral=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.ShowUsers.1", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.ShowUsers.1", static_cast<int>(AuthEventsRecorder::kOffline),
      1);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 1);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason",
      static_cast<int>(SuccessReason::OFFLINE_AND_ONLINE), 1);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/true);
  recorder_->OnUserCount(/*user_count=*/3);
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_AND_ONLINE,
                            /*is_new_user=*/false, /*is_login_offline=*/false,
                            /*is_ephemeral=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.ShowUsers.Few", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.ShowUsers.Few",
      static_cast<int>(AuthEventsRecorder::kOnlineExisting), 1);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 2);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason",
      static_cast<int>(SuccessReason::OFFLINE_AND_ONLINE), 2);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/true);
  recorder_->OnUserCount(/*user_count=*/33);
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_AND_ONLINE,
                            /*is_new_user=*/true, /*is_login_offline=*/false,
                            /*is_ephemeral=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.ShowUsers.Many", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.ShowUsers.Many",
      static_cast<int>(AuthEventsRecorder::kOnlineNew), 1);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 3);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason",
      static_cast<int>(SuccessReason::OFFLINE_AND_ONLINE), 3);
}

TEST_F(AuthEventsRecorderTest, LoginFlowHideUsers) {
  base::HistogramTester histogram_tester;

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/false);
  recorder_->OnUserCount(/*user_count=*/1);
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_AND_ONLINE,
                            /*is_new_user=*/false, /*is_login_offline=*/true,
                            /*is_ephemeral=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.HideUsers.1", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.HideUsers.1", static_cast<int>(AuthEventsRecorder::kOffline),
      1);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 1);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason",
      static_cast<int>(SuccessReason::OFFLINE_AND_ONLINE), 1);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/false);
  recorder_->OnUserCount(/*user_count=*/3);
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_AND_ONLINE,
                            /*is_new_user=*/false, /*is_login_offline=*/false,
                            /*is_ephemeral=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.HideUsers.Few", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.HideUsers.Few",
      static_cast<int>(AuthEventsRecorder::kOnlineExisting), 1);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 2);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason",
      static_cast<int>(SuccessReason::OFFLINE_AND_ONLINE), 2);

  recorder_->ResetLoginData();
  recorder_->OnShowUsersOnSignin(/*show_users_on_signin=*/false);
  recorder_->OnUserCount(/*user_count=*/33);
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_AND_ONLINE,
                            /*is_new_user=*/true, /*is_login_offline=*/false,
                            /*is_ephemeral=*/false);
  histogram_tester.ExpectTotalCount("Login.Flow.HideUsers.Many", 1);
  histogram_tester.ExpectBucketCount(
      "Login.Flow.HideUsers.Many",
      static_cast<int>(AuthEventsRecorder::kOnlineNew), 1);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 3);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason",
      static_cast<int>(SuccessReason::OFFLINE_AND_ONLINE), 3);
}

TEST_F(AuthEventsRecorderTest, OnExistingUserLoginExit) {
  base::HistogramTester histogram_tester;

  int two_attempts = 2;
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
  recorder_->OnExistingUserLoginExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, two_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilSuccess", two_attempts, 1);

  int three_attempts = 3;
  recorder_->OnExistingUserLoginExit(
      AuthEventsRecorder::AuthenticationOutcome::kFailure, three_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilFailure", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilFailure", three_attempts, 1);

  int eleven_attempts = 11;
  recorder_->OnExistingUserLoginExit(
      AuthEventsRecorder::AuthenticationOutcome::kRecovery, eleven_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilRecovery", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilRecovery", eleven_attempts, 1);

  int five_attempts = 5;
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLock);
  recorder_->OnExistingUserLoginExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, five_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", five_attempts, 1);

  int seven_attempts = 7;
  recorder_->OnExistingUserLoginExit(
      AuthEventsRecorder::AuthenticationOutcome::kFailure, seven_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilFailure", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilFailure", seven_attempts, 1);
}

// User exits the login/lock screen without any failed attempts.
TEST_F(AuthEventsRecorderTest, OnExistingUserLoginExitWithNoFailure) {
  base::HistogramTester histogram_tester;

  int zero_attempts = 0;
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLock);
  recorder_->OnExistingUserLoginExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, zero_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", zero_attempts, 1);
}

TEST_F(AuthEventsRecorderTest, RecordUserAuthFactors) {
  base::HistogramTester histogram_tester;

  std::vector<cryptohome::AuthFactorType> factors{
      cryptohome::AuthFactorType::kPassword, cryptohome::AuthFactorType::kPin,
      cryptohome::AuthFactorType::kRecovery};
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
  recorder_->RecordUserAuthFactors(factors);

  // The following factors are recorded with `true`.
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.GaiaPassword", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.CryptohomePin", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.Recovery", 1, 1);

  // The following factors are recorded with `false`.
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.SmartCard", 0, 1);
}

TEST_F(AuthEventsRecorderTest, OnRecoveryDone) {
  base::HistogramTester histogram_tester;

  auto one_second = base::Seconds(1);
  recorder_->OnRecoveryDone(
      AuthEventsRecorder::CryptohomeRecoveryResult::kSucceeded, one_second);
  histogram_tester.ExpectBucketCount(
      "Login.CryptohomeRecoveryResult",
      static_cast<int>(
          AuthEventsRecorder::CryptohomeRecoveryResult::kSucceeded),
      1);
  histogram_tester.ExpectTimeBucketCount(
      "Login.CryptohomeRecoveryDuration.Success", one_second, 1);

  auto two_seconds = base::Seconds(2);
  recorder_->OnRecoveryDone(
      AuthEventsRecorder::CryptohomeRecoveryResult::kRecoveryFatalError,
      two_seconds);
  histogram_tester.ExpectBucketCount(
      "Login.CryptohomeRecoveryResult",
      static_cast<int>(
          AuthEventsRecorder::CryptohomeRecoveryResult::kRecoveryFatalError),
      1);
  histogram_tester.ExpectTimeBucketCount(
      "Login.CryptohomeRecoveryDuration.Failure", two_seconds, 1);
}

TEST_F(AuthEventsRecorderTest, SessionStateCrashKey) {
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(GetSessionStateCrashKeyValue(), "login_primary");

  session_manager_->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(GetSessionStateCrashKeyValue(), "locked");

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(GetSessionStateCrashKeyValue(), "active");

  session_manager_->SetSessionState(session_manager::SessionState::UNKNOWN);
  EXPECT_EQ(GetSessionStateCrashKeyValue(), "unknown");
}

}  // namespace ash
