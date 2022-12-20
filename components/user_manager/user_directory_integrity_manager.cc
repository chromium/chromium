// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_directory_integrity_manager.h"

#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_manager {

namespace {

const char kUserDirectoryIntegrityAccountPref[] =
    "incomplete_login_user_account";

}  // namespace

UserDirectoryIntegrityManager::UserDirectoryIntegrityManager(
    PrefService* local_state)
    : local_state_(local_state) {}
UserDirectoryIntegrityManager::~UserDirectoryIntegrityManager() = default;

// static
void UserDirectoryIntegrityManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kUserDirectoryIntegrityAccountPref, {});
}

void UserDirectoryIntegrityManager::RecordCreatingNewUser(
    const AccountId& account_id) {
  local_state_->SetString(kUserDirectoryIntegrityAccountPref,
                          account_id.GetUserEmail());
  local_state_->CommitPendingWrite();
}

void UserDirectoryIntegrityManager::ClearKnownUserPrefs() {
  absl::optional<AccountId> account_id = GetMisconfiguredUser();
  DCHECK(account_id);
  UserManager::Get()->RemoveUserFromList(account_id.value());
}

void UserDirectoryIntegrityManager::ClearPrefs() {
  local_state_->ClearPref(kUserDirectoryIntegrityAccountPref);
  local_state_->CommitPendingWrite();
}

absl::optional<AccountId>
UserDirectoryIntegrityManager::GetMisconfiguredUser() {
  auto incomplete_user_email =
      local_state_->GetString(kUserDirectoryIntegrityAccountPref);
  return incomplete_user_email.empty()
             ? absl::nullopt
             : absl::optional<AccountId>(
                   AccountId::FromUserEmail(incomplete_user_email));
}

}  // namespace user_manager
