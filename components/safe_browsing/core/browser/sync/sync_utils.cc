// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/sync_utils.h"

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"

namespace safe_browsing {

// static
bool SyncUtils::IsPrimaryAccountSignedIn(
    signin::IdentityManager* identity_manager) {
  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return !primary_account_info.account_id.empty();
}

// static
bool SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    bool user_has_enabled_enhanced_protection) {
  // If the user has explicitly enabled enhanced protection and the primary
  // account is available, no further conditions are needed.
  if (user_has_enabled_enhanced_protection &&
      IsPrimaryAccountSignedIn(identity_manager)) {
    return true;
  }

  // Otherwise, check the status of sync: Safe browsing token fetches are
  // enabled when the user is syncing their browsing history without a custom
  // passphrase.
  // NOTE: |sync_service| can be null in Incognito, and can also be set to null
  // by a cmdline param, but `GetUploadToGoogleState` handles that.
  return (syncer::GetUploadToGoogleState(
              sync_service, syncer::DataType::HISTORY_DELETE_DIRECTIVES) ==
          syncer::UploadState::ACTIVE) &&
         !sync_service->GetUserSettings()->IsUsingExplicitPassphrase();
}

// TODO(bdea): Migrate other SB classes that define this method to call the one
// here instead.
bool SyncUtils::IsHistorySyncEnabled(syncer::SyncService* sync_service) {
  return sync_service && !sync_service->IsLocalSyncEnabled() &&
         sync_service->GetActiveDataTypes().Has(
             syncer::HISTORY_DELETE_DIRECTIVES);
}

}  // namespace safe_browsing
