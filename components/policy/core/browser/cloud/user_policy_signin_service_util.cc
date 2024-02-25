// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"

#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/network_change_notifier.h"

namespace policy {

bool IsSignoutEvent(const signin::PrimaryAccountChangeEvent& event) {
  return event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
         signin::PrimaryAccountChangeEvent::Type::kCleared;
}

bool IsTurnOffSyncEvent(const signin::PrimaryAccountChangeEvent& event) {
  // TODO(crbug.com/40066949): Remove kSync usage after users are migrated to
  // kSignin only after kSync sunset. See ConsentLevel::kSync for more details.
  return event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
         signin::PrimaryAccountChangeEvent::Type::kCleared;
}

bool IsAnySigninEvent(const signin::PrimaryAccountChangeEvent& event) {
  // TODO(crbug.com/40066949): Remove kSync usage after users are migrated to
  // kSignin only after kSync sunset. See ConsentLevel::kSync for more details.
  return event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
             signin::PrimaryAccountChangeEvent::Type::kSet ||
         event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
             signin::PrimaryAccountChangeEvent::Type::kSet;
}

bool CanApplyPoliciesForSignedInUser(
    bool check_for_refresh_token,
    signin::ConsentLevel consent_level,
    signin::IdentityManager* identity_manager) {
  return (
      check_for_refresh_token
          ? identity_manager->HasPrimaryAccountWithRefreshToken(consent_level)
          : identity_manager->HasPrimaryAccount(consent_level));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

base::Time GetLastPolicyCheckTimeFromPrefs(PrefService* prefs) {
  return base::Time::FromInternalValue(
      prefs->GetInt64(policy::policy_prefs::kLastPolicyCheckTime));
}

void UpdateLastPolicyCheckTimeInPrefs(PrefService* prefs) {
  // Persist the current time as the last policy registration attempt time.
  prefs->SetInt64(policy_prefs::kLastPolicyCheckTime,
                  base::Time::Now().ToInternalValue());
}

base::TimeDelta GetTryRegistrationDelayFromPrefs(PrefService* prefs) {
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  base::TimeDelta retry_delay = base::Days(3);
  if (connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
      connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    retry_delay = base::Days(1);
  }

  base::Time last_check_time = GetLastPolicyCheckTimeFromPrefs(prefs);
  base::Time now = base::Time::Now();
  base::Time next_check_time = last_check_time + retry_delay;

  // If the current timestamp (|now|) falls between |last_check_time| and
  // |next_check_time|, return the necessary |try_registration_delay| to reach
  // |next_check_time| from current time (|now|)). Returns the default
  // |try_registration_delay| otherwise to perform the overdue registration
  // asap.
  base::TimeDelta try_registration_delay = base::Seconds(5);
  if (now > last_check_time && now < next_check_time)
    try_registration_delay = next_check_time - now;

  return try_registration_delay;
}

#endif

}  // namespace policy
