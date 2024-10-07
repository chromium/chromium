// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_directory_integrity_manager.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/account_id_util.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"

namespace user_manager {
namespace {

// Initial version of the preference, contained String value of user's e-mail.
// This value is not written by the code, but we need to read it in case when
// device restart also resulted in applying OS update.
const char kUserDirectoryIntegrityAccountPref[] =
    "incomplete_login_user_account";

// Updated version of the preference, contains a Dict that is used to serialize
// AccountId, and might contain additional information.
const char kUserDirectoryIntegrityAccountPrefV2[] =
    "incomplete_login_user_account_v2";
const char kCleanupStrategyKey[] = "cleanup_strategy";

}  // namespace

using CleanupStrategy = UserDirectoryIntegrityManager::CleanupStrategy;

UserDirectoryIntegrityManager::UserDirectoryIntegrityManager(
    PrefService* local_state)
    : local_state_(local_state) {}
UserDirectoryIntegrityManager::~UserDirectoryIntegrityManager() = default;

// static
void UserDirectoryIntegrityManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kUserDirectoryIntegrityAccountPref, {});
  registry->RegisterDictionaryPref(kUserDirectoryIntegrityAccountPrefV2, {});
}

void UserDirectoryIntegrityManager::RecordCreatingNewUser(
    const AccountId& account_id,
    CleanupStrategy strategy) {
  LOG(WARNING) << "Creating new user, don't have credentials yet.";
  base::Value::Dict serialized_account;
  StoreAccountId(account_id, serialized_account);
  serialized_account.Set(kCleanupStrategyKey, static_cast<int>(strategy));
  local_state_->SetDict(kUserDirectoryIntegrityAccountPrefV2,
                        std::move(serialized_account));
  local_state_->CommitPendingWrite();
}

void UserDirectoryIntegrityManager::RemoveUser(const AccountId& account_id) {
  UserManager::Get()->RemoveUser(account_id,
                                 UserRemovalReason::MISCONFIGURED_USER);
}

void UserDirectoryIntegrityManager::ClearPrefs() {
  LOG(WARNING) << "Created user have credentials now.";
  local_state_->ClearPref(kUserDirectoryIntegrityAccountPref);
  local_state_->ClearPref(kUserDirectoryIntegrityAccountPrefV2);
  local_state_->CommitPendingWrite();
}

std::optional<AccountId>
UserDirectoryIntegrityManager::GetMisconfiguredUserAccountId() {
  const base::Value::Dict& account_dict =
      local_state_->GetDict(kUserDirectoryIntegrityAccountPrefV2);
  std::optional<AccountId> result = LoadAccountId(account_dict);
  if (result) {
    return result;
  }
  return GetMisconfiguredUserAccountIdLegacy();
}

CleanupStrategy
UserDirectoryIntegrityManager::GetMisconfiguredUserCleanupStrategy() {
  const base::Value::Dict& account_dict =
      local_state_->GetDict(kUserDirectoryIntegrityAccountPrefV2);
  std::optional<int> raw_strategy = account_dict.FindInt(kCleanupStrategyKey);
  if (raw_strategy) {
    CHECK(0 <= *raw_strategy);
    CHECK(*raw_strategy <= static_cast<int>(CleanupStrategy::kMaxValue));
    return static_cast<CleanupStrategy>(*raw_strategy);
  }
  // Default value
  return CleanupStrategy::kRemoveUser;
}

std::optional<AccountId>
UserDirectoryIntegrityManager::GetMisconfiguredUserAccountIdLegacy() {
  std::optional<std::string> misconfigured_user_email =
      GetMisconfiguredUserEmail();

  if (!misconfigured_user_email.has_value()) {
    return std::nullopt;
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
    // loading process in `UserManagerImpl::EnsureUsersLoaded`, to prevent
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
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::optional<std::string>
UserDirectoryIntegrityManager::GetMisconfiguredUserEmail() {
  auto incomplete_user_email =
      local_state_->GetString(kUserDirectoryIntegrityAccountPref);
  return incomplete_user_email.empty()
             ? std::nullopt
             : std::make_optional(incomplete_user_email);
}

bool UserDirectoryIntegrityManager::IsUserMisconfigured(
    const AccountId& account_id) {
  const base::Value::Dict& account_dict =
      local_state_->GetDict(kUserDirectoryIntegrityAccountPrefV2);
  if (!account_dict.empty()) {
    return AccountIdMatches(account_id, account_dict);
  }
  // Legacy option.
  std::optional<std::string> incomplete_user_email =
      GetMisconfiguredUserEmail();
  return incomplete_user_email.has_value() &&
         incomplete_user_email == account_id.GetUserEmail();
}

}  // namespace user_manager
