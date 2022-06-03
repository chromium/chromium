// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_account_storage_settings_watcher.h"

#include <utility>

#include "base/callback.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync/driver/sync_service.h"

namespace password_manager {

PasswordAccountStorageSettingsWatcher::PasswordAccountStorageSettingsWatcher(
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    base::RepeatingClosure change_callback)
    : sync_service_(sync_service),
      change_callback_(std::move(change_callback)) {
  DCHECK(pref_service);
  // The opt-in state is per-account, so it can change whenever the state of the
  // account used by Sync changes.
  if (sync_service_)
    sync_service_->AddObserver(this);
  // The opt-in state is stored in a pref, so changes to the pref might indicate
  // a change to the opt-in state.
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      password_manager::prefs::kAccountStoragePerAccountSettings,
      change_callback_);
}

PasswordAccountStorageSettingsWatcher::
    ~PasswordAccountStorageSettingsWatcher() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

void PasswordAccountStorageSettingsWatcher::OnStateChanged(
    syncer::SyncService* sync_service) {
  change_callback_.Run();
}

}  // namespace password_manager
