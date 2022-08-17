// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_directory_integrity_manager.h"

#include "components/prefs/pref_service.h"

namespace user_manager {

namespace {

const char kUserDirectoryIntegrityPref[] = "incomplete_login_user";

}  // namespace

UserDirectoryIntegrityManager::UserDirectoryIntegrityManager(
    PrefService* local_state)
    : local_state_(local_state) {}
UserDirectoryIntegrityManager::~UserDirectoryIntegrityManager() = default;

// static
void UserDirectoryIntegrityManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kUserDirectoryIntegrityPref, {});
}

void UserDirectoryIntegrityManager::RecordCreatingNewUser(
    const AccountId& account_id) {
  local_state_->SetString(kUserDirectoryIntegrityPref,
                          account_id.GetUserEmail());
  local_state_->CommitPendingWrite();
}

void UserDirectoryIntegrityManager::RecordAuthFactorAdded(
    const AccountId& account_id) {
  local_state_->ClearPref(kUserDirectoryIntegrityPref);
  local_state_->CommitPendingWrite();
}

std::string UserDirectoryIntegrityManager::GetIncompleteUser() {
  return local_state_->GetString(kUserDirectoryIntegrityPref);
}

}  // namespace user_manager
