// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_auth_util.h"

#include "base/feature_list.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"

namespace syncer {

SyncAccountInfo::SyncAccountInfo() = default;

SyncAccountInfo::SyncAccountInfo(const CoreAccountInfo& account_info,
                                 bool is_sync_consented)
    : account_info(account_info), is_sync_consented(is_sync_consented) {}

SyncAccountInfo DetermineAccountToUse(
    signin::IdentityManager* identity_manager) {
  // TODO(crbug.com/1383977): During signout, it can happen that the primary
  // account temporarily doesn't have a refresh token (before the account
  // itself gets removed). As a workaround for crbug.com/1383912 /
  // crbug.com/897628, do *not* use the account for Sync in this case. This
  // ensures that Sync metadata gets properly cleared during signout.
  if (identity_manager->AreRefreshTokensLoaded() &&
      !identity_manager->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin) &&
      base::FeatureList::IsEnabled(kSyncIgnoreAccountWithoutRefreshToken)) {
    return SyncAccountInfo();
  }

  return SyncAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
      /*is_sync_consented=*/identity_manager->HasPrimaryAccount(
          signin::ConsentLevel::kSync));
}

}  // namespace syncer
