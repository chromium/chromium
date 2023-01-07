// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/entry_point_display_reason.h"

#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

bool ShouldOfferSignin(syncer::SyncService* sync_service,
                       PrefService* pref_service) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return false;
#else
  return pref_service->GetBoolean(prefs::kSigninAllowed) && sync_service &&
         sync_service->GetAccountInfo().IsEmpty() &&
         !sync_service->IsLocalSyncEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace

absl::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    const GURL& url_to_share,
    syncer::SyncService* sync_service,
    SendTabToSelfSyncService* send_tab_to_self_sync_service,
    PrefService* pref_service) {
  if (!url_to_share.SchemeIsHTTPOrHTTPS())
    return absl::nullopt;

  if (!send_tab_to_self_sync_service) {
    // Can happen in incognito or guest profile.
    return absl::nullopt;
  }

  if (ShouldOfferSignin(sync_service, pref_service) &&
      base::FeatureList::IsEnabled(kSendTabToSelfSigninPromo)) {
    return EntryPointDisplayReason::kOfferSignIn;
  }

  SendTabToSelfModel* model =
      send_tab_to_self_sync_service->GetSendTabToSelfModel();
  if (!model->IsReady())
    return absl::nullopt;

  if (!model->HasValidTargetDevice()) {
    return base::FeatureList::IsEnabled(kSendTabToSelfSigninPromo)
               ? absl::make_optional(
                     EntryPointDisplayReason::kInformNoTargetDevice)
               : absl::nullopt;
  }

  return EntryPointDisplayReason::kOfferFeature;
}

}  // namespace send_tab_to_self
