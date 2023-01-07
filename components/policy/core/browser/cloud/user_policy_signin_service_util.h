// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/policy_export.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace policy {

// Returns true if the account was signed out.
POLICY_EXPORT bool IsSignoutEvent(
    const signin::PrimaryAccountChangeEvent& event);

// Returns true if sync was turned off for the account.
POLICY_EXPORT bool IsTurnOffSyncEvent(
    const signin::PrimaryAccountChangeEvent& event);

// Returns true if the event is related to sign-in.
POLICY_EXPORT bool IsAnySigninEvent(
    const signin::PrimaryAccountChangeEvent& event);

// Returns true if policies can be applied for the signed in user.
POLICY_EXPORT bool CanApplyPoliciesForSignedInUser(
    bool check_for_refresh_token,
    signin::ConsentLevel consent_level,
    signin::IdentityManager* identity_manager);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Gets the timestamp representing the last time the registration was done.
POLICY_EXPORT base::Time GetLastPolicyCheckTimeFromPrefs(PrefService* prefs);

// Updates the timestamp representing the last time the registration was done
// with the current time.
POLICY_EXPORT void UpdateLastPolicyCheckTimeInPrefs(PrefService* prefs);

// Gets the delay between each registration try. Used for mobile to throttle
// network calls.
POLICY_EXPORT base::TimeDelta GetTryRegistrationDelayFromPrefs(
    PrefService* prefs);

#endif

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_
