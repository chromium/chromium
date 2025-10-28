// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_sign_in_state_watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

HistorySignInState GetHistorySignInState(
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service) {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    if (!signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
            sync_service, syncer::UserSelectableTypeSet(
                              {syncer::UserSelectableType::kTabs}))) {
      return HistorySignInState::kSyncDisabled;
    }

    switch (signin_util::GetSignedInState(identity_manager)) {
      case signin_util::SignedInState::kSignedOut:
        return HistorySignInState::kSignedOut;

      case signin_util::SignedInState::kWebOnlySignedIn:
        return HistorySignInState::kWebOnlySignedIn;

      case signin_util::SignedInState::kSignInPending:
        return sync_service &&
                       sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kTabs)
                   ? HistorySignInState::kSignInPendingSyncingTabs
                   : HistorySignInState::kSignInPendingNotSyncingTabs;

      case signin_util::SignedInState::kSignedIn:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
        if (signin_util::HasExplicitlyDisabledHistorySync(sync_service,
                                                          identity_manager)) {
          return HistorySignInState::kSyncDisabled;
        }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
        return sync_service &&
                       sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kTabs)
                   ? HistorySignInState::kSignedInSyncingTabs
                   : HistorySignInState::kSignedInNotSyncingTabs;

      case signin_util::SignedInState::kSyncing:
      case signin_util::SignedInState::kSyncPaused:
        return sync_service &&
                       sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kTabs)
                   ? HistorySignInState::kSignedInSyncingTabs
                   : HistorySignInState::kSyncDisabled;
    }
  } else {
    // Note: This intentionally does not check whether the history data type is
    // actually enabled (for historical reasons, mostly).
    return identity_manager && identity_manager->HasPrimaryAccount(
                                   signin::ConsentLevel::kSync)
               ? HistorySignInState::kSignedInSyncingTabs
               : HistorySignInState::kSignedOut;
  }
}

HistorySignInStateWatcher::HistorySignInStateWatcher(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    base::RepeatingClosure callback)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      callback_(std::move(callback)),
      cached_signin_state_(GetSignInState()) {
  DCHECK(!callback_.is_null());

  if (sync_service_) {
    sync_observation_.Observe(sync_service_);
  }

  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
}

HistorySignInStateWatcher::~HistorySignInStateWatcher() = default;

void HistorySignInStateWatcher::OnStateChanged(syncer::SyncService* sync) {
  UpdateSignInState();
}

void HistorySignInStateWatcher::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdateSignInState();
}

void HistorySignInStateWatcher::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateSignInState();
}

void HistorySignInStateWatcher::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateSignInState();
}

void HistorySignInStateWatcher::UpdateSignInState() {
  HistorySignInState signin_state = GetSignInState();
  if (signin_state == cached_signin_state_) {
    return;
  }

  cached_signin_state_ = signin_state;
  RunCallback();
}

void HistorySignInStateWatcher::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
}

void HistorySignInStateWatcher::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

HistorySignInState HistorySignInStateWatcher::GetSignInState() const {
  return GetHistorySignInState(identity_manager_, sync_service_);
}

void HistorySignInStateWatcher::RunCallback() {
  callback_.Run();
}
