// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_directory_integrity_manager.h"

#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
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

void UserDirectoryIntegrityManager::RemoveUser(const AccountId& account_id) {
  UserManager::Get()->RemoveUser(
      account_id, UserRemovalReason::MISCONFIGURED_USER, /*delegate=*/nullptr);
}

void UserDirectoryIntegrityManager::ClearPrefs() {
  local_state_->ClearPref(kUserDirectoryIntegrityAccountPref);
  local_state_->CommitPendingWrite();
}

absl::optional<AccountId>
UserDirectoryIntegrityManager::GetMisconfiguredUserAccountId() {
  absl::optional<std::string> misconfigured_user_email =
      GetMisconfiguredUserEmail();

  if (!misconfigured_user_email.has_value()) {
    return absl::nullopt;
  }

  UserList users = UserManager::Get()->GetUsers();
  auto misconfigured_user_it =
      base::ranges::find_if(users, [&misconfigured_user_email](User* user) {
        return user->GetAccountId().GetUserEmail() ==
               misconfigured_user_email.value();
      });

  if (misconfigured_user_it == std::end(users)) {
    // If the user was not found in the list, then it's a regular user and not a
    // Kiosk user, since regular misconfigured users are skipped during the
    // loading process in `UserManagerBase::EnsureUsersLoaded`, to prevent
    // showing them on the login screen
    user_manager::KnownUser known_user(local_state_);
    user_manager::CryptohomeId cryptohome_id(misconfigured_user_email.value());
    return known_user.GetAccountIdByCryptohomeId(cryptohome_id);
  }

  if (User* misconfigured_user = *misconfigured_user_it;
      misconfigured_user->IsDeviceLocalAccount() &&
      misconfigured_user->IsKioskType()) {
    return misconfigured_user->GetAccountId();
  }

  // Since we only record `incomplete_login_user_account` pref in
  // `auth_session_authenticator` for regular and kiosk users, it should be
  // impossible to reach here after checking for both types of users above.
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<std::string>
UserDirectoryIntegrityManager::GetMisconfiguredUserEmail() {
  auto incomplete_user_email =
      local_state_->GetString(kUserDirectoryIntegrityAccountPref);
  return incomplete_user_email.empty()
             ? absl::nullopt
             : absl::make_optional(incomplete_user_email);
}

bool UserDirectoryIntegrityManager::IsUserMisconfigured(
    const AccountId& account_id) {
  absl::optional<std::string> incomplete_user_email =
      GetMisconfiguredUserEmail();
  return incomplete_user_email.has_value() &&
         incomplete_user_email == account_id.GetUserEmail();
}

}  // namespace user_manager
