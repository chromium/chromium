// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_events_recorder.h"

#include <numeric>
#include <optional>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/crash/core/common/crash_key.h"

namespace ash {
namespace {

using AuthenticationSurface = AuthEventsRecorder::AuthenticationSurface;
using AuthenticationOutcome = AuthEventsRecorder::AuthenticationOutcome;
using CryptohomeRecoveryResult = AuthEventsRecorder::CryptohomeRecoveryResult;
using UserLoginType = AuthEventsRecorder::UserLoginType;

// Constants for crash keys:
constexpr int kMaxSessionStateCrashKeyLength = 32;
constexpr int kMaxAuthEventsCrashKeyLength = 1024;
constexpr char kAuthEventSeparator = ',';

// Names of the crash keys:
constexpr char kAuthEventsCrashKey[] = "auth-events";
constexpr char kSessionStateCrashKey[] = "session-state";

// Histogram for tracking the reason of auth failure
constexpr char kFailureReasonHistogramName[] = "Login.FailureReason";

// Histogram for tracking the reason of login success
constexpr char kSuccessReasonHistogramName[] = "Login.SuccessReason";

// Histogram prefix for tracking login flow. The format:
// "Login.Flow.{HideUsers,ShowUsers}.{0,1,2,Few,Many}"
constexpr char kLoginFlowHistogramPrefix[] = "Login.Flow.";

// Histogram for the number of password attempts on the login/lock screen exit.
// The format:
// "Ash.OSAuth.{Login,Lock}.NbPasswordAttempts.{UntilFailure,UntilSuccess}"
constexpr char kNbPasswordAttemptsHistogramName[] =
    "Ash.OSAuth.%s.NbPasswordAttempts.%s";

constexpr char kRecoveryResultHistogramName[] =
    "Login.CryptohomeRecoveryResult";

constexpr char kRecoveryDurationHistogramPrefix[] =
    "Login.CryptohomeRecoveryDuration.";

// Limit definition of "many users"
constexpr int kManyUserLimit = 5;

// Histogram prefix for recording the configured auth factors. The format:
// "Ash.OSAuth.Login.ConfiguredAuthFactors.{Pin,Password,...}"
constexpr char kConfiguredAuthFactorsHistogramPrefix[] =
    "Ash.OSAuth.Login.ConfiguredAuthFactors.";

// Histogram prefix for recording duration of various login flow phases.
// Format: "Ash.OSAuth.Login.Times.{...}"
constexpr char kLoginTimeHistogramPrefix[] = "Ash.OSAuth.Login.Times.";

constexpr char kLoginTimeFactorConfigTotal[] = "FactorConfigTotal";
constexpr char kLoginTimeEarlyPrefsReadSuffix[] = "EarlyPrefsRead";
constexpr char kLoginTimeEarlyPrefsParseSuffix[] = "EarlyPrefsParse";
constexpr char kLoginTimeFactorMigraionsSuffix[] = "FactorMigrations";
constexpr char kLoginTimePolicyEnforcementSuffix[] = "PolicyEnforcement";

// The auth factors tracked for "Ash.OSAuth.Login.ConfiguredAuthFactors.*"
// histogram reporting. When adding new values here, update
// `GetConfiguredAuthFactorsHistogramSuffix` and
// metadata/ash/histograms.xml.
const auto kTrackedAuthFactors = {cryptohome::AuthFactorType::kPassword,
                                  cryptohome::AuthFactorType::kPin,
                                  cryptohome::AuthFactorType::kRecovery,
                                  cryptohome::AuthFactorType::kSmartCard};

// Suffix for grouping total user numbers. Should match suffixes of the
// Login.Flow.{HideUsers, ShowUsers}.* metrics in metadata/ash/histograms.xml
std::string ShowUserPrefix(bool show_users_on_signin) {
  return show_users_on_signin ? "ShowUsers." : "HideUsers.";
}

// Suffix for grouping user counts. Should match suffixes of the
// Login.Flow.{HideUsers, ShowUsers}.* metrics in metadata/ash/histograms.xml
std::string UserCountSuffix(int user_count) {
  DCHECK_GE(user_count, 0);
  if (user_count <= 0) {
    return "0";
  }

  if (user_count == 1) {
    return "1";
  }

  if (user_count == 2) {
    return "2";
  }

  if (user_count < kManyUserLimit) {
    return "Few";
  }

  return "Many";
}

// Suffix for grouping by screen type. Should match suffixes of the
// Ash.OSAuth.{Login,Lock}.NbPasswordAttempts.{UntilFailure,UntilSuccess}
// metrics in metadata/ash/histograms.xml
std::string GetAuthenticationSurfaceName(AuthenticationSurface screen) {
  switch (screen) {
    case AuthenticationSurface::kLock:
      return "Lock";
    case AuthenticationSurface::kLogin:
      return "Login";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

// Suffix for grouping by screen exit type. Should match suffixes of the
// Ash.OSAuth.{Login,Lock}.NbPasswordAttempts.{UntilFailure,UntilSuccess}
// metrics in metadata/ash/histograms.xml
std::string GetAuthenticationOutcomeSuffix(AuthenticationOutcome exit_type) {
  switch (exit_type) {
    case AuthenticationOutcome::kSuccess:
      return "UntilSuccess";
    case AuthenticationOutcome::kFailure:
      return "UntilFailure";
    case AuthenticationOutcome::kRecovery:
      return "UntilRecovery";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

// Complete name of the login flow histogram.
std::string GetLoginFlowHistogramName(bool show_users_on_signin,
                                      int user_count) {
  return base::StrCat({kLoginFlowHistogramPrefix,
                       ShowUserPrefix(show_users_on_signin),
                       UserCountSuffix(user_count)});
}

// Complete name of the number of password attempts histogram.
std::string GetNbPasswordAttemptsHistogramName(
    AuthenticationSurface screen,
    AuthenticationOutcome exit_type) {
  return base::StringPrintf(kNbPasswordAttemptsHistogramName,
                            GetAuthenticationSurfaceName(screen).c_str(),
                            GetAuthenticationOutcomeSuffix(exit_type).c_str());
}

// Should match suffixes of the
// "Ash.OSAuth.Login.ConfiguredAuthFactors.{Pin,Password,...}"
// metrics in metadata/ash/histograms.xml.
std::string GetConfiguredAuthFactorsHistogramSuffix(
    cryptohome::AuthFactorType factor) {
  CHECK_NE(factor, cryptohome::AuthFactorType::kPassword);
  switch (factor) {
    case cryptohome::AuthFactorType::kPin:
      return "CryptohomePin";
    case cryptohome::AuthFactorType::kRecovery:
      return "Recovery";
    case cryptohome::AuthFactorType::kSmartCard:
      return "SmartCard";
    case cryptohome::AuthFactorType::kPassword:
      NOTREACHED_IN_MIGRATION()
          << "For password factor use "
             "`GetConfiguredPasswordFactorsHistogramSuffix()`";
      return "";
    case cryptohome::AuthFactorType::kUnknownLegacy:
    case cryptohome::AuthFactorType::kLegacyFingerprint:
    case cryptohome::AuthFactorType::kFingerprint:
    case cryptohome::AuthFactorType::kKiosk:
      // These factors are not recorded.
      DCHECK(false);
      return "";
  }
  return "";
}

// Complete name of the configured auth factors histogram.
std::string GetConfiguredAuthFactorsHistogramName(
    cryptohome::AuthFactorType factor) {
  CHECK_NE(factor, cryptohome::AuthFactorType::kPassword)
      << "For password factor use `GetConfiguredPasswordFactorHistogramName()`";
  return base::StrCat(
      {kConfiguredAuthFactorsHistogramPrefix,
       GetConfiguredAuthFactorsHistogramSuffix(factor).c_str()});
}

enum class ConfiguredPasswordType {
  kGaia,
  kLocal,
};

// Should match suffixes of the
// "Ash.OSAuth.Login.ConfiguredAuthFactors.{GaiaPassword,LocalPassword...}"
// metrics in metadata/ash/histograms.xml.
std::string GetConfiguredPasswordFactorsHistogramSuffix(
    const ConfiguredPasswordType& type) {
  switch (type) {
    case ConfiguredPasswordType::kGaia:
      return "GaiaPassword";
    case ConfiguredPasswordType::kLocal:
      return "LocalPassword";
  }
}

// Complete name of the configured auth factors histogram.
std::string GetConfiguredPasswordFactorHistogramName(
    const ConfiguredPasswordType& type) {
  return base::StrCat(
      {kConfiguredAuthFactorsHistogramPrefix,
       GetConfiguredPasswordFactorsHistogramSuffix(type).c_str()});
}

std::string GetRecoveryOutcomeSuffix(CryptohomeRecoveryResult result) {
  return result == CryptohomeRecoveryResult::kSucceeded ? "Success" : "Failure";
}

std::string GetRecoveryDurationHistogramName(CryptohomeRecoveryResult result) {
  return base::StrCat(
      {kRecoveryDurationHistogramPrefix, GetRecoveryOutcomeSuffix(result)});
}

// Values of the `kSessionStateCrashKey`. The length should not exceed
// `kMaxSessionStateCrashKeyLength`.
std::string GetSessionStateCrashKeyValue(session_manager::SessionState state) {
  switch (state) {
    case session_manager::SessionState::ACTIVE:
      return "active";
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      return "logged_in_not_active";
    case session_manager::SessionState::LOCKED:
      return "locked";
    case session_manager::SessionState::LOGIN_PRIMARY:
      return "login_primary";
    case session_manager::SessionState::LOGIN_SECONDARY:
      return "login_secondary";
    case session_manager::SessionState::OOBE:
      return "oobe";
    case session_manager::SessionState::RMA:
      return "rma";
    case session_manager::SessionState::UNKNOWN:
      return "unknown";
  }
}

std::string GetUserLoginTypeName(AuthEventsRecorder::UserLoginType type) {
  switch (type) {
    case UserLoginType::kOnlineNew:
      return "online_new";
    case UserLoginType::kOnlineExisting:
      return "online_existing";
    case UserLoginType::kOffline:
      return "offline";
    case UserLoginType::kEphemeral:
      return "ephemeral";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string GetAuthenticationOutcomeName(AuthenticationOutcome exit_type) {
  switch (exit_type) {
    case AuthenticationOutcome::kSuccess:
      return "success";
    case AuthenticationOutcome::kFailure:
      return "failure";
    case AuthenticationOutcome::kRecovery:
      return "recovery";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string GetUserVaultTypeName(
    AuthEventsRecorder::UserVaultType user_vault_type) {
  using UserVaultType = AuthEventsRecorder::UserVaultType;
  switch (user_vault_type) {
    case UserVaultType::kPersistent:
      return "persistent";
    case UserVaultType::kEphemeral:
      return "ephemeral";
    case UserVaultType::kGuest:
      return "guest";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string GetCrashKeyStringWithStatus(const std::string& event_name,
                                        bool success) {
  return event_name + (success ? "_success" : "_failure");
}

}  // namespace

// static
AuthEventsRecorder* AuthEventsRecorder::instance_ = nullptr;

AuthEventsRecorder::AuthEventsRecorder() {
  DCHECK(!instance_);
  instance_ = this;
  // Note: SessionManager may be nullptr in tests.
  if (!session_manager::SessionManager::Get()) {
    LOG(WARNING) << "Failed to observe SessionManager";
    CHECK_IS_TEST();
    return;
  }
  session_observation_.Observe(session_manager::SessionManager::Get());
  // Set the initial value.
  OnSessionStateChanged();
}

AuthEventsRecorder::~AuthEventsRecorder() {
  instance_ = nullptr;
}

// static
AuthEventsRecorder* AuthEventsRecorder::Get() {
  CHECK(instance_) << "If there is no instance in test, use "
                      "AuthEventsRecorder::CreateForTesting()";
  return instance_;
}

// static
std::unique_ptr<ash::AuthEventsRecorder>
AuthEventsRecorder::CreateForTesting() {
  return base::WrapUnique<AuthEventsRecorder>(new AuthEventsRecorder());
}

void AuthEventsRecorder::ResetLoginData() {
  Reset();
}

void AuthEventsRecorder::OnKnowledgeFactorAuthFailure() {
  knowledge_factor_auth_failure_count_++;
}

void AuthEventsRecorder::OnAuthFailure(
    const AuthFailure::FailureReason& reason) {
  base::RecordAction(base::UserMetricsAction("Login_Failure"));
  UMA_HISTOGRAM_ENUMERATION(kFailureReasonHistogramName, reason,
                            AuthFailure::NUM_FAILURE_REASONS);
  AddAuthEvent(GetCrashKeyStringWithStatus("login", /*success=*/false));
}

void AuthEventsRecorder::OnLoginSuccess(const SuccessReason& reason,
                                        bool is_new_user,
                                        bool is_login_offline,
                                        bool is_ephemeral) {
  base::RecordAction(base::UserMetricsAction("Login_Success"));
  UMA_HISTOGRAM_ENUMERATION(kSuccessReasonHistogramName, reason,
                            SuccessReason::NUM_SUCCESS_REASONS);
  UpdateUserLoginType(is_new_user, is_login_offline, is_ephemeral);
}

void AuthEventsRecorder::OnGuestLoginSuccess() {
  base::RecordAction(base::UserMetricsAction("Login_GuestLoginSuccess"));
  AddAuthEvent(GetCrashKeyStringWithStatus("guest_login", /*success=*/true));
}

void AuthEventsRecorder::OnUserCount(int user_count) {
  user_count_ = user_count;
  MaybeReportFlowMetrics();
}

void AuthEventsRecorder::OnShowUsersOnSignin(bool show_users_on_signin) {
  show_users_on_signin_ = show_users_on_signin;
  MaybeReportFlowMetrics();
}

void AuthEventsRecorder::OnAuthenticationSurfaceChange(
    AuthenticationSurface surface) {
  auth_surface_ = surface;
  AddAuthEvent("auth_surface_change_" + GetAuthenticationSurfaceName(surface));
}

void AuthEventsRecorder::OnExistingUserLoginScreenExit(
    AuthenticationOutcome exit_type,
    int num_login_attempts) {
  CHECK(auth_surface_);
  CHECK_GE(num_login_attempts, 0);
  if (exit_type == AuthenticationOutcome::kFailure) {
    CHECK_NE(num_login_attempts, 0);
  }

  base::UmaHistogramCounts100(
      GetNbPasswordAttemptsHistogramName(auth_surface_.value(), exit_type),
      num_login_attempts);
  AddAuthEvent("login_screen_exit_" + GetAuthenticationOutcomeName(exit_type));
}

void AuthEventsRecorder::RecordSessionAuthFactors(
    const SessionAuthFactors& auth_factors) const {
  // These histograms are recorded only for login at the moment.
  // If we need to record the auth factors configured for unlock as well, the
  // DCHECK can be removed and `auth_surface_` value can be used to determine
  // the metrics type.
  DCHECK(auth_surface_.has_value());
  DCHECK_EQ(auth_surface_.value(), AuthenticationSurface::kLogin);

  const auto factor_types = auth_factors.GetSessionFactors();
  for (const auto factor : kTrackedAuthFactors) {
    if (factor == cryptohome::AuthFactorType::kPassword) {
      auto* online_password = auth_factors.FindOnlinePasswordFactor();
      base::UmaHistogramBoolean(GetConfiguredPasswordFactorHistogramName(
                                    ConfiguredPasswordType::kGaia),
                                online_password != nullptr);
      auto* local_password = auth_factors.FindLocalPasswordFactor();
      base::UmaHistogramBoolean(GetConfiguredPasswordFactorHistogramName(
                                    ConfiguredPasswordType::kLocal),
                                local_password != nullptr);
      continue;
    }

    base::UmaHistogramBoolean(GetConfiguredAuthFactorsHistogramName(factor),
                              base::Contains(factor_types, factor));
  }
}

void AuthEventsRecorder::OnRecoveryDone(CryptohomeRecoveryResult result,
                                        const base::TimeDelta& time) {
  base::UmaHistogramMediumTimes(GetRecoveryDurationHistogramName(result), time);
  base::UmaHistogramEnumeration(kRecoveryResultHistogramName, result);
  AddAuthEvent(GetCrashKeyStringWithStatus(
      "recovery_done", result == CryptohomeRecoveryResult::kSucceeded));
}

void AuthEventsRecorder::OnAuthSubmit() {
  AddAuthEvent("auth_submit");
}

void AuthEventsRecorder::OnAuthComplete(std::optional<bool> auth_success) {
  const std::string auth_complete_str = "auth_complete";
  if (!auth_success.has_value()) {
    AddAuthEvent(auth_complete_str);
    return;
  }
  AddAuthEvent(
      GetCrashKeyStringWithStatus(auth_complete_str, auth_success.value()));
}

void AuthEventsRecorder::OnPinSubmit() {
  AddAuthEvent("pin_submit");
}

void AuthEventsRecorder::OnLockContentsViewUpdate() {
  AddAuthEvent("update_lock_screen_view");
}

void AuthEventsRecorder::OnPasswordChange() {
  AddAuthEvent("password_change");
}

void AuthEventsRecorder::OnGaiaScreen() {
  AddAuthEvent("gaia");
}

void AuthEventsRecorder::OnUserVaultPrepared(UserVaultType user_vault_type,
                                             bool success) {
  const std::string crash_key_prefix =
      GetUserVaultTypeName(user_vault_type) + "_vault_prepare";
  AddAuthEvent(GetCrashKeyStringWithStatus(crash_key_prefix, success));
}

void AuthEventsRecorder::OnAddUser() {
  AddAuthEvent("add_user");
}

std::string AuthEventsRecorder::GetAuthEventsLog() {
  // Preallocate the space needed for all the events combined.
  const size_t events_string_length =
      std::accumulate(events_.begin(), events_.end(), 0,
                      [](const size_t sum, const std::string& event) {
                        return sum + event.length() + 1;
                      });
  std::stringstream result_string;
  for (std::string_view event : events_) {
    result_string << event;
    result_string << kAuthEventSeparator;
  }
  DCHECK_EQ(result_string.str().length(), events_string_length);
  return result_string.str();
}

void AuthEventsRecorder::OnSessionStateChanged() {
  TRACE_EVENT0("login", "AuthEventsRecorder::OnSessionStateChanged");
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  static crash_reporter::CrashKeyString<kMaxSessionStateCrashKeyLength> key(
      kSessionStateCrashKey);
  key.Set(GetSessionStateCrashKeyValue(session_state));
}

void AuthEventsRecorder::UpdateUserLoginType(bool is_new_user,
                                             bool is_login_offline,
                                             bool is_ephemeral) {
  if (is_login_offline) {
    user_login_type_ = UserLoginType::kOffline;
  } else if (!is_new_user) {
    // The rest 3 online login types are with either existing user and new users
    user_login_type_ = UserLoginType::kOnlineExisting;
  } else if (is_ephemeral) {
    // The rest 2 new user login types are either ephemeral or new online users
    user_login_type_ = UserLoginType::kEphemeral;
  } else {
    user_login_type_ = UserLoginType::kOnlineNew;
  }

  AddAuthEvent("login_" + GetUserLoginTypeName(user_login_type_.value()));
  MaybeReportFlowMetrics();
}

void AuthEventsRecorder::MaybeReportFlowMetrics() {
  if (!show_users_on_signin_.has_value() || !user_count_.has_value() ||
      !user_login_type_.has_value()) {
    return;
  }

  base::UmaHistogramEnumeration(
      GetLoginFlowHistogramName(show_users_on_signin_.value(),
                                user_count_.value()),
      user_login_type_.value());
}

void AuthEventsRecorder::AddAuthEvent(const std::string& event_name) {
  events_.push_back(event_name);
  UpdateAuthEventsCrashKey();
}

void AuthEventsRecorder::UpdateAuthEventsCrashKey() {
  if (events_.size() == 0) {
    return;
  }

  std::string crash_key_string = GetAuthEventsLog();
  if (crash_key_string.length() > kMaxAuthEventsCrashKeyLength) {
    crash_key_string = crash_key_string.substr(crash_key_string.length() -
                                               kMaxAuthEventsCrashKeyLength);
  }

  // Note: the string will be truncated to `kMaxAuthEventsCrashKeyLength`.
  static crash_reporter::CrashKeyString<kMaxAuthEventsCrashKeyLength> key(
      kAuthEventsCrashKey);
  key.Set(crash_key_string);
}

void AuthEventsRecorder::Reset() {
  user_count_ = std::nullopt;
  show_users_on_signin_ = std::nullopt;
  user_login_type_ = std::nullopt;
  auth_surface_ = std::nullopt;
  knowledge_factor_auth_failure_count_ = 0;
}

void AuthEventsRecorder::StartPostLoginFactorAdjustments() {
  factor_adjustment_start_ = base::TimeTicks::Now();
  last_adjustment_event_ = factor_adjustment_start_;
}

void AuthEventsRecorder::OnEarlyPrefsRead() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_adjustment_event_) {
    base::TimeDelta diff = now - *last_adjustment_event_;
    base::UmaHistogramTimes(base::StrCat({kLoginTimeHistogramPrefix,
                                          kLoginTimeEarlyPrefsReadSuffix}),
                            diff);
  }
  last_adjustment_event_ = now;
}

void AuthEventsRecorder::OnEarlyPrefsParsed() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_adjustment_event_) {
    base::TimeDelta diff = now - *last_adjustment_event_;
    base::UmaHistogramTimes(base::StrCat({kLoginTimeHistogramPrefix,
                                          kLoginTimeEarlyPrefsParseSuffix}),
                            diff);
  }
  last_adjustment_event_ = now;
}

void AuthEventsRecorder::OnFactorUpdateStarted() {
  base::TimeTicks now = base::TimeTicks::Now();
  last_adjustment_event_ = now;
}

void AuthEventsRecorder::OnMigrationsCompleted() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_adjustment_event_) {
    base::TimeDelta diff = now - *last_adjustment_event_;
    base::UmaHistogramTimes(base::StrCat({kLoginTimeHistogramPrefix,
                                          kLoginTimeFactorMigraionsSuffix}),
                            diff);
  }
  last_adjustment_event_ = now;
}

void AuthEventsRecorder::OnPoliciesApplied() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_adjustment_event_) {
    base::TimeDelta diff = now - *last_adjustment_event_;
    base::UmaHistogramTimes(base::StrCat({kLoginTimeHistogramPrefix,
                                          kLoginTimePolicyEnforcementSuffix}),
                            diff);
  }
  last_adjustment_event_ = now;
}

void AuthEventsRecorder::FinishPostLoginFactorAdjustments() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (factor_adjustment_start_) {
    base::TimeDelta diff = now - *factor_adjustment_start_;
    base::UmaHistogramTimes(
        base::StrCat({kLoginTimeHistogramPrefix, kLoginTimeFactorConfigTotal}),
        diff);
  }
  factor_adjustment_start_.reset();
  last_adjustment_event_.reset();
}

}  // namespace ash
