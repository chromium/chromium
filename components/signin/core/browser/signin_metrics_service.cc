// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics_service.h"

#include <optional>
#include <string_view>

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kExplicitSigninMigrationHistogramName[] =
    "Signin.ExplicitSigninMigration";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {

constexpr char kSigninPendingStartTimePref[] =
    "signin.signin_pending_start_time";

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// This pref contains the web signin start time of the accounts that have signed
// on the web only. If the account is removed or any account gets signed in to
// Chrome, the pref is cleared.
// The pref is a dictionary that maps the account ids to the web signin start
// time per account.
// Storing the account_id is not ideal as it might not be consistent with
// different platforms, however it is fine the purpose of this metric.
constexpr char kWebSigninAccountStartTimesPref[] =
    "signin.web_signin_accounts_start_time_dict";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninPendingResolution {
  kReauth = 0,
  kSignout = 1,

  kMaxValue = kSignout,
};

void RecordSigninPendingResolution(SigninPendingResolution resolution,
                                   base::Time signin_pending_start_time) {
  base::UmaHistogramEnumeration("Signin.SigninPending.Resolution", resolution);

  std::string_view resolution_string;
  switch (resolution) {
    case SigninPendingResolution::kReauth:
      resolution_string = "Reauth";
      break;
    case SigninPendingResolution::kSignout:
      resolution_string = "Signout";
      break;
  }

  std::string histogram_resolution_time_name =
      base::StrCat({"Signin.SigninPending.ResolutionTime.", resolution_string});

  base::TimeDelta time_in_signin_pending =
      base::Time::Now() - signin_pending_start_time;
  base::UmaHistogramCustomTimes(histogram_resolution_time_name,
                                time_in_signin_pending, base::Seconds(0),
                                base::Days(14), 50);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Metric is recorded for specific access points.
void MaybeRecordWebSigninToChromeSigninTimes(
    base::Time web_signin_start_time,
    signin_metrics::AccessPoint access_point) {
  std::string_view access_point_string;
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
      access_point_string = "ProfileMenu";
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
      access_point_string = "PasswordSigninPromo";
      break;
    default:
      // All other access point should not record this metric.
      return;
  }

  std::string histogram_web_signin_to_chrome_signin_time_name = base::StrCat(
      {"Signin.WebSignin.TimeToChromeSignin.", access_point_string});
  base::TimeDelta time_in_web_signin_only_until_chrome_signin =
      base::Time::Now() - web_signin_start_time;

  base::UmaHistogramCustomTimes(histogram_web_signin_to_chrome_signin_time_name,
                                time_in_web_signin_only_until_chrome_signin,
                                base::Seconds(0), base::Days(7), 50);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

SigninMetricsService::SigninMetricsService(
    signin::IdentityManager& identity_manager,
    PrefService& pref_service)
    : identity_manager_(identity_manager), pref_service_(pref_service) {
  identity_manager_scoped_observation_.Observe(&identity_manager_.get());

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  RecordExplicitSigninMigrationStatus();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

SigninMetricsService::~SigninMetricsService() = default;

// static
void SigninMetricsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kSigninPendingStartTimePref, base::Time());
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterDictionaryPref(kWebSigninAccountStartTimesPref);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

void SigninMetricsService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      std::optional<signin_metrics::AccessPoint> access_point =
          event_details.GetAccessPoint();
      CHECK(access_point.has_value());

      MaybeRecordWebSigninToChromeSigninMetrics(
          event_details.GetCurrentState().primary_account.account_id,
          *access_point);

      RecordSigninInterceptionMetrics(
          event_details.GetCurrentState().primary_account.gaia, *access_point);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

      return;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      if (pref_service_->HasPrefPath(kSigninPendingStartTimePref)) {
        RecordSigninPendingResolution(
            SigninPendingResolution::kSignout,
            pref_service_->GetTime(kSigninPendingStartTimePref));
        pref_service_->ClearPref(kSigninPendingStartTimePref);
      }
      return;
  }
}

void SigninMetricsService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& core_account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled() ||
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return;
  }

  if (core_account_info !=
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)) {
    return;
  }

  if (error.IsPersistentError()) {
    if (!pref_service_->HasPrefPath(kSigninPendingStartTimePref)) {
      pref_service_->SetTime(kSigninPendingStartTimePref, base::Time::Now());
    }
  } else if (pref_service_->HasPrefPath(kSigninPendingStartTimePref)) {
    RecordSigninPendingResolution(
        SigninPendingResolution::kReauth,
        pref_service_->GetTime(kSigninPendingStartTimePref));
    pref_service_->ClearPref(kSigninPendingStartTimePref);

    AccountInfo account_info =
        identity_manager_->FindExtendedAccountInfo(core_account_info);
    if (account_info.access_point !=
        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN) {
      // Only record `Started` from WEB_SIGNIN, since there is no way to know
      // that a WebSignin resolution has started until it was completed. Other
      // access points are client access points which can be tracked at the
      // real started event.
      if (account_info.access_point ==
          signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
        base::UmaHistogramEnumeration(
            "Signin.SigninPending.ResolutionSourceStarted",
            account_info.access_point,
            signin_metrics::AccessPoint::ACCESS_POINT_MAX);
      }

      base::UmaHistogramEnumeration(
          "Signin.SigninPending.ResolutionSourceCompleted",
          account_info.access_point,
          signin_metrics::AccessPoint::ACCESS_POINT_MAX);
    }
  }
}

void SigninMetricsService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
      info.access_point ==
          signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN &&
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    ScopedDictPrefUpdate update(&pref_service_.get(),
                                kWebSigninAccountStartTimesPref);
    update->Set(info.account_id.ToString(),
                base::TimeToValue(base::Time::Now()));
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

void SigninMetricsService::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& core_account_id) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (pref_service_->HasPrefPath(kWebSigninAccountStartTimesPref)) {
    ScopedDictPrefUpdate update(&pref_service_.get(),
                                kWebSigninAccountStartTimesPref);
    update->Remove(core_account_id.ToString());
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

void SigninMetricsService::RecordExplicitSigninMigrationStatus() {
  ExplicitSigninMigration explicit_signin_migration =
      ExplicitSigninMigration::kMigratedSignedOut;
  const bool explicit_signin_pref =
      pref_service_->GetBoolean(prefs::kExplicitBrowserSignin);
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    explicit_signin_migration =
        explicit_signin_pref ? ExplicitSigninMigration::kMigratedSyncing
                             : ExplicitSigninMigration::kNotMigratedSyncing;
  } else if (identity_manager_->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
    explicit_signin_migration =
        explicit_signin_pref ? ExplicitSigninMigration::kMigratedSignedIn
                             : ExplicitSigninMigration::kNotMigratedSignedIn;
  }

  base::UmaHistogramEnumeration(kExplicitSigninMigrationHistogramName,
                                explicit_signin_migration);
}

void SigninMetricsService::MaybeRecordWebSigninToChromeSigninMetrics(
    const CoreAccountId& account_id,
    signin_metrics::AccessPoint access_point) {
  if (pref_service_->HasPrefPath(kWebSigninAccountStartTimesPref)) {
    const base::Value::Dict& web_signin_account_start_time_dict =
        pref_service_->GetDict(kWebSigninAccountStartTimesPref);

    // This value only exists if the initial signin was from a web signin
    // source.
    const base::Value* start_time_value =
        web_signin_account_start_time_dict.Find(account_id.ToString());
    std::optional<base::Time> start_time =
        start_time_value ? base::ValueToTime(start_time_value) : std::nullopt;
    if (start_time.has_value()) {
      MaybeRecordWebSigninToChromeSigninTimes(start_time.value(), access_point);

      base::UmaHistogramEnumeration(
          "Signin.WebSignin.SourceToChromeSignin", access_point,
          signin_metrics::AccessPoint::ACCESS_POINT_MAX);
    }
    // Clear all related web signin information on the first Chrome signin
    // event.
    pref_service_->ClearPref(kWebSigninAccountStartTimesPref);
  }
}

void SigninMetricsService::RecordSigninInterceptionMetrics(
    const std::string& gaia_id,
    signin_metrics::AccessPoint access_point) {
  ChromeSigninUserChoice signin_choice =
      SigninPrefs(pref_service_.get())
          .GetChromeSigninInterceptionUserChoice(gaia_id);
  base::UmaHistogramEnumeration("Signin.Settings.ChromeSignin.OnSignin",
                                signin_choice);
  if (signin_choice == ChromeSigninUserChoice::kDoNotSignin) {
    base::UmaHistogramEnumeration(
        "Signin.Settings.ChromeSignin.AccessPointWithDoNotSignin", access_point,
        signin_metrics::AccessPoint::ACCESS_POINT_MAX);
  }
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
