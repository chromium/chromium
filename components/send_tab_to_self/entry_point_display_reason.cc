// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/entry_point_display_reason.h"

#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

bool ShouldOfferSignin(syncer::SyncService* sync_service,
                       PrefService* pref_service) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return false;
#else
  return pref_service->GetBoolean(prefs::kSigninAllowed) &&
         sync_service->GetAccountInfo().IsEmpty() &&
         !sync_service->HasDisableReason(
             syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) &&
         !sync_service->IsLocalSyncEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace

namespace internal {

std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    const GURL& url_to_share,
    syncer::SyncService* sync_service,
    SendTabToSelfModel* send_tab_to_self_model,
    PrefService* pref_service) {
  if (!url_to_share.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }

  if (!send_tab_to_self_model || !sync_service) {
    // Send-tab-to-self can't work properly, don't show the entry point.
    return std::nullopt;
  }

  if (ShouldOfferSignin(sync_service, pref_service)) {
    return EntryPointDisplayReason::kOfferSignIn;
  }

  if (!send_tab_to_self_model->IsReady()) {
    syncer::SyncUserSettings* settings = sync_service->GetUserSettings();
    if (sync_service->IsEngineInitialized() &&
        (settings->IsPassphraseRequiredForPreferredDataTypes() ||
         settings->IsTrustedVaultKeyRequiredForPreferredDataTypes())) {
      // There's an encryption error, the model won't become ready unless the
      // user takes explicit action. But the error will be surfaced by dedicated
      // non send-tab-to-self UI. So just treat this as the no device case.
      return EntryPointDisplayReason::kInformNoTargetDevice;
    }
    return std::nullopt;
  }

  if (!send_tab_to_self_model->HasValidTargetDevice()) {
    return EntryPointDisplayReason::kInformNoTargetDevice;
  }

  return EntryPointDisplayReason::kOfferFeature;
}

}  // namespace internal

}  // namespace send_tab_to_self
