// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_auth_util.h"

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace syncer {

SyncAccountInfo::SyncAccountInfo() = default;

SyncAccountInfo::SyncAccountInfo(const CoreAccountInfo& account_info,
                                 bool is_sync_consented)
    : account_info(account_info), is_sync_consented(is_sync_consented) {}

SyncAccountInfo DetermineAccountToUse(
    signin::IdentityManager* identity_manager) {
  return SyncAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
      /*is_sync_consented=*/identity_manager->HasPrimaryAccount(
          signin::ConsentLevel::kSync));
}

}  // namespace syncer
