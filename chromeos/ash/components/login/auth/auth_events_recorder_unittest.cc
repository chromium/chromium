// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_events_recorder.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

cryptohome::AuthFactor MakeRecoveryFactor() {
  cryptohome::AuthFactorRef ref(
      cryptohome::AuthFactorType::kRecovery,
      cryptohome::KeyLabel(kCryptohomeRecoveryKeyLabel));
  return cryptohome::AuthFactor(std::move(ref),
                                cryptohome::AuthFactorCommonMetadata());
}

cryptohome::AuthFactor MakeGaiaAuthFactor() {
  cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                                cryptohome::KeyLabel(kCryptohomeGaiaKeyLabel));
  return cryptohome::AuthFactor(std::move(ref),
                                cryptohome::AuthFactorCommonMetadata());
}

cryptohome::AuthFactor MakeLocalPasswordFactor() {
  cryptohome::AuthFactorRef ref(
      cryptohome::AuthFactorType::kPassword,
      cryptohome::KeyLabel(kCryptohomeLocalPasswordKeyLabel));
  return cryptohome::AuthFactor(std::move(ref),
                                cryptohome::AuthFactorCommonMetadata());
}

cryptohome::AuthFactor MakePinAuthFactor() {
  cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPin,
                                cryptohome::KeyLabel(kCryptohomePinLabel));
  return cryptohome::AuthFactor(std::move(ref),
                                cryptohome::AuthFactorCommonMetadata());
}

cryptohome::AuthFactor MakeLegacyAuthFactor(int legacy_key_index) {
  cryptohome::AuthFactorRef ref(
      cryptohome::AuthFactorType::kPassword,
      cryptohome::KeyLabel(base::StringPrintf("legacy-%d", legacy_key_index)));
  return cryptohome::AuthFactor(std::move(ref),
                                cryptohome::AuthFactorCommonMetadata());
}

#if !defined(COMPONENT_BUILD)
std::string GetSessionStateCrashKeyValue() {
  return crash_reporter::GetCrashKeyValue("session-state");
}

std::string GetAuthEventsCrashKeyValue() {
  std::string result = crash_reporter::GetCrashKeyValue("auth-events");
  if (!result.empty()) {
    return result;
  }

  // Breakpad breaks the crash key value up into chunks into chunks labeled
  // name__1 through name__N.
  static constexpr char kCrashKeyName[] = "auth-events__%d";
  std::string chunk;
  int index = 0;
  do {
    chunk = crash_reporter::GetCrashKeyValue(
        base::StringPrintf(kCrashKeyName, ++index));
    result += chunk;
  } while (chunk.length() > 0);
  return result;
}
#endif  // !defined(COMPONENT_BUILD)

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
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
      "Login.Flow.ShowUsers.1",
      static_cast<int>(AuthEventsRecorder::UserLoginType::kOffline), 1);
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
      static_cast<int>(AuthEventsRecorder::UserLoginType::kOnlineExisting), 1);
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
      static_cast<int>(AuthEventsRecorder::UserLoginType::kOnlineNew), 1);
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
      "Login.Flow.HideUsers.1",
      static_cast<int>(AuthEventsRecorder::UserLoginType::kOffline), 1);
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
      static_cast<int>(AuthEventsRecorder::UserLoginType::kOnlineExisting), 1);
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
      static_cast<int>(AuthEventsRecorder::UserLoginType::kOnlineNew), 1);
  histogram_tester.ExpectTotalCount("Login.SuccessReason", 3);
  histogram_tester.ExpectBucketCount(
      "Login.SuccessReason",
      static_cast<int>(SuccessReason::OFFLINE_AND_ONLINE), 3);
}

TEST_F(AuthEventsRecorderTest, OnExistingUserLoginScreenExit) {
  base::HistogramTester histogram_tester;

  int two_attempts = 2;
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, two_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilSuccess", two_attempts, 1);

  int three_attempts = 3;
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kFailure, three_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilFailure", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilFailure", three_attempts, 1);

  int eleven_attempts = 11;
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kRecovery, eleven_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilRecovery", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.NbPasswordAttempts.UntilRecovery", eleven_attempts, 1);

  int five_attempts = 5;
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLock);
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, five_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", five_attempts, 1);

  int seven_attempts = 7;
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kFailure, seven_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilFailure", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilFailure", seven_attempts, 1);
}

// User exits the login/lock screen without any failed attempts.
TEST_F(AuthEventsRecorderTest, OnExistingUserLoginScreenExitWithNoFailure) {
  base::HistogramTester histogram_tester;

  int zero_attempts = 0;
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLock);
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, zero_attempts);
  histogram_tester.ExpectTotalCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Lock.NbPasswordAttempts.UntilSuccess", zero_attempts, 1);
}

TEST_F(AuthEventsRecorderTest, RecordSessionAuthFactors) {
  base::HistogramTester histogram_tester;

  SessionAuthFactors factors(
      {MakeGaiaAuthFactor(), MakePinAuthFactor(), MakeRecoveryFactor()});
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
  recorder_->RecordSessionAuthFactors(factors);

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
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.LocalPassword", 0, 1);
}

TEST_F(AuthEventsRecorderTest, RecordSessionAuthFactorsLocalPassword) {
  base::HistogramTester histogram_tester;

  SessionAuthFactors factors(
      {MakeLocalPasswordFactor(), MakePinAuthFactor(), MakeRecoveryFactor()});
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
  recorder_->RecordSessionAuthFactors(factors);

  // The following factors are recorded with `true`.
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.LocalPassword", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.CryptohomePin", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.Recovery", 1, 1);

  // The following factors are recorded with `false`.
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.SmartCard", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.GaiaPassword", 0, 1);
}

TEST_F(AuthEventsRecorderTest, RecordSessionAuthFactorsLegacyPassword) {
  base::HistogramTester histogram_tester;

  SessionAuthFactors factors(
      {MakeLegacyAuthFactor(1), MakePinAuthFactor(), MakeRecoveryFactor()});
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
  recorder_->RecordSessionAuthFactors(factors);

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
  histogram_tester.ExpectBucketCount(
      "Ash.OSAuth.Login.ConfiguredAuthFactors.LocalPassword", 0, 1);
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

// These tests fail on the component build because
// GetSessionStateCrashKeyValue() doesn't pull from the same crashpad instance
// that is used by AuthEventsRecorder.
#if !defined(COMPONENT_BUILD)
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

TEST_F(AuthEventsRecorderTest, AuthEventsCrashKeyOnSuccessfullLogin) {
  // Login screen:
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
  recorder_->OnLockContentsViewUpdate();
  recorder_->OnAuthSubmit();
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_ONLY,
                            /*is_new_user=*/false, /*is_login_offline=*/true,
                            /*is_ephemeral=*/false);
  recorder_->OnAuthComplete(true);
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, 1);
  EXPECT_EQ(GetAuthEventsCrashKeyValue(),
            "auth_surface_change_Login,update_lock_screen_view,auth_submit,"
            "login_offline,auth_complete_success,login_screen_exit_success,");
  // Lock screen:
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLock);
  recorder_->OnLockContentsViewUpdate();
  // 3 failed attempts:
  recorder_->OnAuthSubmit();
  recorder_->OnAuthFailure(
      AuthFailure::FailureReason::COULD_NOT_MOUNT_CRYPTOHOME);
  recorder_->OnAuthComplete(false);
  recorder_->OnAuthSubmit();
  recorder_->OnAuthFailure(
      AuthFailure::FailureReason::COULD_NOT_MOUNT_CRYPTOHOME);
  recorder_->OnAuthComplete(false);
  recorder_->OnAuthSubmit();
  recorder_->OnAuthFailure(
      AuthFailure::FailureReason::COULD_NOT_MOUNT_CRYPTOHOME);
  recorder_->OnAuthComplete(false);
  // 1 successfull attempt:
  recorder_->OnAuthSubmit();
  recorder_->OnLoginSuccess(SuccessReason::OFFLINE_ONLY,
                            /*is_new_user=*/false, /*is_login_offline=*/true,
                            /*is_ephemeral=*/false);
  recorder_->OnAuthComplete(true);
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, 4);
  EXPECT_EQ(
      GetAuthEventsCrashKeyValue(),
      "auth_surface_change_Login,update_lock_screen_view,auth_submit,"
      "login_offline,auth_complete_success,login_screen_exit_success,"
      "auth_surface_change_Lock,update_lock_screen_view,auth_submit,"
      "login_failure,auth_complete_failure,auth_submit,login_failure,"
      "auth_complete_failure,auth_submit,login_failure,auth_complete_failure,"
      "auth_submit,login_offline,auth_complete_success,"
      "login_screen_exit_success,");
}

TEST_F(AuthEventsRecorderTest, AuthEventsCrashKeyOnSuccessfullUnlock) {
  // Lock screen:
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLock);
  recorder_->OnLockContentsViewUpdate();
  recorder_->OnAuthSubmit();
  recorder_->OnAuthComplete(true);
  recorder_->OnExistingUserLoginScreenExit(
      AuthEventsRecorder::AuthenticationOutcome::kSuccess, 4);
  EXPECT_EQ(GetAuthEventsCrashKeyValue(),
            "auth_surface_change_Lock,update_lock_screen_view,auth_submit,"
            "auth_complete_success,login_screen_exit_success,");
}

TEST_F(AuthEventsRecorderTest, AuthEventsCrashKeyOnFailureUnlock) {
  // Lock screen:
  recorder_->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLock);
  recorder_->OnLockContentsViewUpdate();
  recorder_->OnAuthSubmit();
  recorder_->OnAuthComplete(false);
  EXPECT_EQ(GetAuthEventsCrashKeyValue(),
            "auth_surface_change_Lock,update_lock_screen_view,auth_submit,"
            "auth_complete_failure,");
}

TEST_F(AuthEventsRecorderTest, PostFactorsAdjustmentTimings) {
  base::HistogramTester histogram_tester;

  recorder_->StartPostLoginFactorAdjustments();
  task_environment_.AdvanceClock(base::Milliseconds(10));
  recorder_->OnEarlyPrefsRead();
  task_environment_.AdvanceClock(base::Milliseconds(20));
  recorder_->OnEarlyPrefsParsed();
  // This time does not have individual bucket, but it falls
  // into the total bucket.
  task_environment_.AdvanceClock(base::Milliseconds(40));
  recorder_->OnFactorUpdateStarted();
  task_environment_.AdvanceClock(base::Milliseconds(80));
  recorder_->OnMigrationsCompleted();
  task_environment_.AdvanceClock(base::Milliseconds(160));
  recorder_->OnPoliciesApplied();
  recorder_->FinishPostLoginFactorAdjustments();

  histogram_tester.ExpectTimeBucketCount(
      "Ash.OSAuth.Login.Times.FactorConfigTotal",
      base::Milliseconds(10 + 20 + 40 + 80 + 160), 1);
  histogram_tester.ExpectTimeBucketCount(
      "Ash.OSAuth.Login.Times.EarlyPrefsRead", base::Milliseconds(10), 1);
  histogram_tester.ExpectTimeBucketCount(
      "Ash.OSAuth.Login.Times.EarlyPrefsParse", base::Milliseconds(20), 1);
  histogram_tester.ExpectTimeBucketCount(
      "Ash.OSAuth.Login.Times.FactorMigrations", base::Milliseconds(80), 1);
  histogram_tester.ExpectTimeBucketCount(
      "Ash.OSAuth.Login.Times.PolicyEnforcement", base::Milliseconds(160), 1);
}

#endif  // !defined(COMPONENT_BUILD)

}  // namespace ash
