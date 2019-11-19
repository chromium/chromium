// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_auth_util.h"

#include <vector>

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace syncer {

SyncAccountInfo::SyncAccountInfo() = default;

SyncAccountInfo::SyncAccountInfo(const CoreAccountInfo& account_info,
                                 bool is_primary)
    : account_info(account_info), is_primary(is_primary) {}

SyncAccountInfo DetermineAccountToUse(
    signin::IdentityManager* identity_manager) {
  return SyncAccountInfo(identity_manager->GetUnconsentedPrimaryAccountInfo(),
                         /*is_primary=*/identity_manager->HasPrimaryAccount());
}

bool IsWebSignout(const GoogleServiceAuthError& auth_error) {
  // The identity code sets an account's refresh token to be invalid (error
  // CREDENTIALS_REJECTED_BY_CLIENT) if the user signs out of that account on
  // the web.
  return auth_error ==
         GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
             GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                 CREDENTIALS_REJECTED_BY_CLIENT);
}

}  // namespace syncer
