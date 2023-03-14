// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_METRICS_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_METRICS_RECORDER_H_

#include <vector>

#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

// This class encapsulates metrics reporting. User actions and behaviors are
// reported in multiple stages of the login flow. This metrics reporter would
// centralize the tracking and reporting.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) AuthMetricsRecorder {
 public:
  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update LoginFlowUserLoginType enum in enums.xml when adding new entries.
  enum UserLoginType {
    kOnlineNew = 0,
    kOnlineExisting = 1,
    kOffline = 2,
    kEphemeral = 3,
    kMaxValue
  };

  enum class AuthenticationSurface { kLogin, kLock };
  enum class AuthenticationOutcome {
    // User successfully logged in.
    kSuccess,
    // User exited the login screen without successfully logging in.
    kFailure,
    // User opened the account recovery flow from the login screen.
    kRecovery
  };

  // The result of the cryptohome recovery.
  // These values are reported to UMA "Login.CryptohomeRecoveryResult". Entries
  // should not be renumbered and numeric values should never be reused.
  enum class CryptohomeRecoveryResult {
    kSucceeded = 0,
    // Failed to fetch OAuth token with recovery scope.
    kOAuthTokenFetchError = 1,
    // Failed to fetch epoch from the server.
    kEpochFetchError = 2,
    // Failed to get recovery request.
    kGetRecoveryRequestError = 3,
    // Failed to fetch recovery response from the server.
    kRecoveryResponseFetchError = 4,
    // Recovery response received with transient error encoded in the response.
    kRecoveryTransientError = 5,
    // Recovery response received with fatal error encoded in the response.
    kRecoveryFatalError = 6,
    // Failed to authenticate with the recovery factor.
    kAuthenticateRecoveryFactorError = 7,
    // Failed to mount Cryptohome.
    kMountCryptohomeError = 8,
    kMaxValue = kMountCryptohomeError,
  };

  AuthMetricsRecorder(const AuthMetricsRecorder&) = delete;
  AuthMetricsRecorder& operator=(const AuthMetricsRecorder&) = delete;
  AuthMetricsRecorder(AuthMetricsRecorder&&) = delete;
  AuthMetricsRecorder& operator=(AuthMetricsRecorder&&) = delete;
  ~AuthMetricsRecorder();

  static AuthMetricsRecorder* Get();

  static std::unique_ptr<ash::AuthMetricsRecorder> CreateForTesting();

  // Reset the login data (user type, user count etc).
  // Should be called at the beginning of the login.
  void ResetLoginData();

  // Logs the auth failure action and reason.
  void OnAuthFailure(const AuthFailure::FailureReason& failure_reason);

  // Logs the login success action and reason.
  void OnLoginSuccess(const SuccessReason& reason);

  // Logs the guest login success action.
  void OnGuestLoginSuccess();

  // Set the total number of regular users on the lock screen.
  // May log the values to UMA if all information is available.
  void OnUserCount(int user_count);

  // Set the policy setting whether to show users on sign in or not.
  // May log the values to UMA if all information is available.
  void OnShowUsersOnSignin(bool show_users_on_signin);

  // Set the policy setting if ephemeral login are enforced.
  // May log the values to UMA if all information is available.
  void OnEnableEphemeralUsers(bool enable_ephemeral_users);

  // Set whether the last successful login is a new user or not.
  // May log the values to UMA if all information is available.
  void OnIsUserNew(bool is_new_user);

  // Set whether the last successful login is offline or not.
  // May log the values to UMA if all information is available.
  void OnIsLoginOffline(bool is_login_offline);

  // Set the current authentication surface (e.g. login screen, lock screen).
  void OnAuthenticationSurfaceChange(AuthenticationSurface surface);

  // Report how the user exits the login screen and the number of login
  // attempts.
  // `OnAuthenticationSurfaceChange` must be called before this method.
  // Nothing will be recorded if the `exit_type` is `kFailure` and
  // `num_login_attempts` is 0.
  void OnExistingUserLoginExit(AuthenticationOutcome exit_type,
                               int num_login_attempts) const;

  // Report which auth factors the user has configured.
  void RecordUserAuthFactors(
      const std::vector<cryptohome::AuthFactorType>& auth_factors) const;

  // Report the result of the recovery and time taken to UMA.
  void OnRecoveryDone(CryptohomeRecoveryResult result,
                      const base::TimeDelta& time);

 private:
  friend class ChromeBrowserMainPartsAsh;

  // Only ChromeBrowserMainPartsAsh can create an instance, in tests - use
  // `CreateForTesting`.
  AuthMetricsRecorder();

  static AuthMetricsRecorder* instance_;

  // Determine the user login type if 3 information are available:
  // is_login_offline_, is_new_user_, enable_ephemeral_users_.
  void MaybeUpdateUserLoginType();

  // Report the user login type in association with policy and total user count
  // if 3 information are available: user_count_, show_users_on_signin_,
  // user_login_type_.
  void MaybeReportFlowMetrics();

  void Reset();

  // All values should be reset to nullopt in `Reset()`;
  absl::optional<int> user_count_;
  absl::optional<bool> show_users_on_signin_;
  absl::optional<bool> enable_ephemeral_users_;
  absl::optional<bool> is_new_user_;
  absl::optional<bool> is_login_offline_;
  absl::optional<UserLoginType> user_login_type_;
  absl::optional<AuthenticationSurface> auth_surface_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_METRICS_RECORDER_H_
