// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/entry_point_display_reason.h"

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

bool IsSigninPossible(syncer::SyncService* sync_service,
                      PrefService* pref_service) {
  const bool signin_allowed = pref_service->GetBoolean(prefs::kSigninAllowed);
  if (!signin_allowed) {
    return false;
  }

  const bool disabled_by_policy = sync_service->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  if (disabled_by_policy || sync_service->IsLocalSyncEnabled()) {
    return false;
  }

  return true;
}

bool ShouldOfferSignin(syncer::SyncService* sync_service,
                       PrefService* pref_service) {
  return IsSigninPossible(sync_service, pref_service) &&
         sync_service->GetAccountInfo().IsEmpty();
}

bool ShouldOfferReauth(syncer::SyncService* sync_service,
                       PrefService* pref_service) {
#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
  return false;
#else
  if (!base::FeatureList::IsEnabled(kSendTabToSelfEnhancedDesktopUI)) {
    return false;
  }

  return IsSigninPossible(sync_service, pref_service) &&
         sync_service->GetTransportState() ==
             syncer::SyncService::TransportState::PAUSED;
#endif
}

bool HasUserActionableErrorBlockingSendTabToSelf(
    const syncer::SyncService& sync_service) {
  switch (sync_service.GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kNone:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
    case syncer::SyncService::UserActionableError::kBookmarksLimitExceeded:
#if BUILDFLAG(IS_ANDROID)
    case syncer::SyncService::UserActionableError::kNeedsUPMBackendUpgrade:
#endif
      return false;

    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    case syncer::SyncService::UserActionableError::kNeedsSettingsConfirmation:
    case syncer::SyncService::UserActionableError::kUnrecoverableError:
#endif
#if BUILDFLAG(IS_IOS)
    case syncer::SyncService::UserActionableError::kDeviceManagementError:
#endif
      return true;
  }
}

}  // namespace

namespace internal {

std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    const GURL& url_to_share,
    syncer::SyncService* sync_service,
    SendTabToSelfModel* send_tab_to_self_model,
    PrefService* pref_service) {
  if (!SendTabToSelfEntry::IsValidUrl(url_to_share)) {
    return std::nullopt;
  }

  if (!send_tab_to_self_model || !sync_service) {
    // Send-tab-to-self can't work properly, don't show the entry point.
    return std::nullopt;
  }

  if (ShouldOfferSignin(sync_service, pref_service)) {
    return EntryPointDisplayReason::kOfferSignIn;
  }

  if (ShouldOfferReauth(sync_service, pref_service)) {
    return EntryPointDisplayReason::kOfferReauth;
  }

  if (HasUserActionableErrorBlockingSendTabToSelf(*sync_service)) {
    return std::nullopt;
  }

  if (!send_tab_to_self_model->IsReady()) {
    return std::nullopt;
  }

  if (!send_tab_to_self_model->HasValidTargetDevice()) {
    return EntryPointDisplayReason::kInformNoTargetDevice;
  }

  return EntryPointDisplayReason::kOfferFeature;
}

}  // namespace internal

}  // namespace send_tab_to_self
