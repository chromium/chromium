// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_identity_state_watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

HistoryIdentityStateWatcher::HistoryIdentityStateWatcher(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    base::RepeatingClosure callback)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      callback_(std::move(callback)),
      cached_identity_state_(GetHistoryIdentityState()) {
  CHECK(!callback_.is_null());

  if (sync_service_) {
    sync_observation_.Observe(sync_service_);
  }

  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
}

HistoryIdentityStateWatcher::~HistoryIdentityStateWatcher() = default;

HistoryIdentityState HistoryIdentityStateWatcher::GetHistoryIdentityState()
    const {
  HistoryIdentityState identity_state;
  identity_state.sign_in = GetHistorySignInState();
  identity_state.tab_sync =
      GetSyncStateForType(syncer::UserSelectableType::kTabs);
  identity_state.history_sync =
      GetSyncStateForType(syncer::UserSelectableType::kHistory);
  return identity_state;
}

void HistoryIdentityStateWatcher::OnStateChanged(syncer::SyncService* sync) {
  UpdateIdentityState();
}

void HistoryIdentityStateWatcher::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdateIdentityState();
}

void HistoryIdentityStateWatcher::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateIdentityState();
}

void HistoryIdentityStateWatcher::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateIdentityState();
}

void HistoryIdentityStateWatcher::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
}

void HistoryIdentityStateWatcher::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

HistoryIdentityState::SignIn
HistoryIdentityStateWatcher::GetHistorySignInState() const {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    switch (signin_util::GetSignedInState(identity_manager_)) {
      case signin_util::SignedInState::kSignedOut:
        return HistoryIdentityState::SignIn::kSignedOut;

      case signin_util::SignedInState::kWebOnlySignedIn:
        return HistoryIdentityState::SignIn::kWebOnlySignedIn;

      case signin_util::SignedInState::kSignInPending:
        return HistoryIdentityState::SignIn::kSignInPending;

      case signin_util::SignedInState::kSignedIn:
      case signin_util::SignedInState::kSyncing:
      case signin_util::SignedInState::kSyncPaused:
        return HistoryIdentityState::SignIn::kSignedIn;
    }
  } else {
    // Note: This intentionally does not check whether the history data type is
    // actually enabled (for historical reasons, mostly).
    return identity_manager_ && identity_manager_->HasPrimaryAccount(
                                    signin::ConsentLevel::kSync)
               ? HistoryIdentityState::SignIn::kSignedIn
               : HistoryIdentityState::SignIn::kSignedOut;
  }
}

HistoryIdentityState::SyncState
HistoryIdentityStateWatcher::GetSyncStateForType(
    syncer::UserSelectableType type) const {
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // Note: This intentionally does not check whether the history data type is
    // actually enabled (for historical reasons, mostly).
    return identity_manager_ && identity_manager_->HasPrimaryAccount(
                                    signin::ConsentLevel::kSync)
               ? HistoryIdentityState::SyncState::kTurnedOn
               : HistoryIdentityState::SyncState::kTurnedOff;
  }

  // If the type is disabled by policy, we consider the corresponding sync
  // state as disabled.
  if (!signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
          sync_service_, syncer::UserSelectableTypeSet({type}))) {
    return HistoryIdentityState::SyncState::kDisabled;
  }
  const signin_util::SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager_);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // if the promo is related to history type, we need to check if any of the
  // History-related types is explicitly disabled via the toggles.
  if (type == syncer::UserSelectableType::kHistory &&
      signed_in_state == signin_util::SignedInState::kSignedIn &&
      signin_util::HasExplicitlyDisabledHistorySync(sync_service_,
                                                    identity_manager_)) {
    return HistoryIdentityState::SyncState::kDisabled;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (sync_service_ &&
      sync_service_->GetUserSettings()->GetSelectedTypes().Has(type)) {
    return HistoryIdentityState::SyncState::kTurnedOn;
  }
  // sync feature is enabled, but the specific type's sync is disabled.
  if (signed_in_state == signin_util::SignedInState::kSyncing ||
      signed_in_state == signin_util::SignedInState::kSyncPaused) {
    return HistoryIdentityState::SyncState::kDisabled;
  }
  return HistoryIdentityState::SyncState::kTurnedOff;
}



void HistoryIdentityStateWatcher::UpdateIdentityState() {
  HistoryIdentityState identity_state = GetHistoryIdentityState();
  if (identity_state == cached_identity_state_) {
    return;
  }

  cached_identity_state_ = identity_state;
  callback_.Run();
}
