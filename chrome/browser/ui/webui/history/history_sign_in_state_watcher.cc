// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_sign_in_state_watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

HistorySignInState GetHistorySignInState(Profile* profile) {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile);
    // TODO(crbug.com/418144047): Distinguish additional signin states (like
    // signed in without history).
    return sync_service &&
                   sync_service->GetUserSettings()->GetSelectedTypes().Has(
                       syncer::UserSelectableType::kHistory)
               ? HistorySignInState::kSignedIn
               : HistorySignInState::kSignedOut;
  } else {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    // Note: This intentionally does not check whether the history data type is
    // actually enabled (for historical reasons, mostly).
    return identity_manager && identity_manager->HasPrimaryAccount(
                                   signin::ConsentLevel::kSync)
               ? HistorySignInState::kSignedIn
               : HistorySignInState::kSignedOut;
  }
}

HistorySignInStateWatcher::HistorySignInStateWatcher(
    Profile* profile,
    base::RepeatingClosure callback)
    : profile_(profile),
      callback_(std::move(callback)),
      cached_signin_state_(GetSignInState()) {
  DCHECK(profile_);
  DCHECK(!callback_.is_null());

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    sync_observation_.Observe(sync_service);
  }
}

HistorySignInStateWatcher::~HistorySignInStateWatcher() = default;

void HistorySignInStateWatcher::OnStateChanged(syncer::SyncService* sync) {
  HistorySignInState signin_state = GetSignInState();
  if (signin_state == cached_signin_state_) {
    return;
  }

  cached_signin_state_ = signin_state;
  RunCallback();
}

void HistorySignInStateWatcher::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

HistorySignInState HistorySignInStateWatcher::GetSignInState() const {
  return GetHistorySignInState(profile_);
}

void HistorySignInStateWatcher::RunCallback() {
  callback_.Run();
}
