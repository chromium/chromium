// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics_service.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

namespace {

constexpr char kSigninPendingStartTimePref[] =
    "signin.sigin_pending_start_time";
constexpr char kFirstAccountWebSigninStartTimePref[] =
    "signin.first_account_web_signin_start_time";

// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
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

// Metric is recorded for specific access points.
void MaybeRecordWebSigninToChromeSignin(
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

}  // namespace

SigninMetricsService::SigninMetricsService(
    signin::IdentityManager& identity_manager,
    PrefService& pref_service)
    : identity_manager_(identity_manager), pref_service_(pref_service) {
  identity_manager_scoped_observation_.Observe(&identity_manager_.get());
}

SigninMetricsService::~SigninMetricsService() = default;

// static
void SigninMetricsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kSigninPendingStartTimePref, base::Time());
  registry->RegisterTimePref(kFirstAccountWebSigninStartTimePref, base::Time());
}

void SigninMetricsService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (pref_service_->HasPrefPath(kFirstAccountWebSigninStartTimePref)) {
        std::optional<signin_metrics::AccessPoint> access_point =
            event_details.GetAccessPoint();
        CHECK(access_point.has_value());
        MaybeRecordWebSigninToChromeSignin(
            pref_service_->GetTime(kFirstAccountWebSigninStartTimePref),
            access_point.value());
        pref_service_->ClearPref(kFirstAccountWebSigninStartTimePref);
      }
      return;
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

void SigninMetricsService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& cookie_jar,
    const GoogleServiceAuthError& error) {
  if (!cookie_jar.accounts_are_fresh) {
    return;
  }

  // Interested in Web-only signed in.
  if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled() ||
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  if (cookie_jar.signed_in_accounts.empty()) {
    pref_service_->ClearPref(kFirstAccountWebSigninStartTimePref);
    return;
  }

  // Set the web signin time for the first account only.
  if (!pref_service_->HasPrefPath(kFirstAccountWebSigninStartTimePref)) {
    pref_service_->SetTime(kFirstAccountWebSigninStartTimePref,
                           base::Time::Now());
  }
}
