// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_

#include "components/policy/policy_export.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"

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
    signin::IdentityManager* identity_manager);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_
