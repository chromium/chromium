// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace policy {

bool IsSignoutEvent(const signin::PrimaryAccountChangeEvent& event) {
  return event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
         signin::PrimaryAccountChangeEvent::Type::kCleared;
}

bool IsTurnOffSyncEvent(const signin::PrimaryAccountChangeEvent& event) {
  return event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
         signin::PrimaryAccountChangeEvent::Type::kCleared;
}

bool IsAnySigninEvent(const signin::PrimaryAccountChangeEvent& event) {
  return event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
             signin::PrimaryAccountChangeEvent::Type::kSet ||
         event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
             signin::PrimaryAccountChangeEvent::Type::kSet;
}

bool CanApplyPoliciesForSignedInUser(
    bool check_for_refresh_token,
    signin::IdentityManager* identity_manager) {
  return (
      check_for_refresh_token
          ? identity_manager->HasPrimaryAccountWithRefreshToken(
                signin::ConsentLevel::kSignin)
          : identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

}  // namespace policy
