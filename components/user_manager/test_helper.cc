// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/test_helper.h"

#include "base/check_deref.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "components/user_manager/user_names.h"

namespace user_manager {
namespace {

void RegisterPersistedUserInternal(PrefService& local_state,
                                   const AccountId& account_id,
                                   UserType user_type) {
  {
    ScopedListPrefUpdate update(&local_state, prefs::kRegularUsersPref);
    update->Append(account_id.GetUserEmail());
  }
  {
    ScopedDictPrefUpdate update(&local_state, prefs::kUserType);
    update->Set(account_id.GetAccountIdKey(), static_cast<int>(user_type));
  }
  {
    KnownUser known_user(&local_state);
    known_user.UpdateId(account_id);
  }
}

}  // namespace

// static
void TestHelper::RegisterPersistedUser(PrefService& local_state,
                                       const AccountId& account_id) {
  RegisterPersistedUserInternal(local_state, account_id, UserType::kRegular);
}

// static
void TestHelper::RegisterPersistedChildUser(PrefService& local_state,
                                            const AccountId& account_id) {
  RegisterPersistedUserInternal(local_state, account_id, UserType::kChild);
}

// static
void TestHelper::RegisterKioskAppUser(PrefService& local_state,
                                      std::string_view user_id) {
  auto type = policy::GetDeviceLocalAccountType(user_id);
  CHECK_EQ(type, policy::DeviceLocalAccountType::kKioskApp)
      << user_id << " did not satisfy to be used for a kiosk user. "
      << "See policy::GetDeviceLocalAccountType for details";
  ScopedListPrefUpdate update(&local_state,
                              prefs::kDeviceLocalAccountsWithSavedData);
  update->Append(user_id);
}

// static
void TestHelper::RegisterWebKioskAppUser(PrefService& local_state,
                                         std::string_view user_id) {
  auto type = policy::GetDeviceLocalAccountType(user_id);
  CHECK_EQ(type, policy::DeviceLocalAccountType::kWebKioskApp)
      << user_id << " did not satisfy to be used for a web kiosk user. "
      << "See policy::GetDeviceLocalAccountType for details";
  ScopedListPrefUpdate update(&local_state,
                              prefs::kDeviceLocalAccountsWithSavedData);
  update->Append(user_id);
}

// static
void TestHelper::RegisterPublicAccountUser(PrefService& local_state,
                                           std::string_view user_id) {
  auto type = policy::GetDeviceLocalAccountType(user_id);
  CHECK_EQ(type, policy::DeviceLocalAccountType::kPublicSession)
      << user_id << " did not satisfy to be used for a public account user. "
      << "See policy::GetDeviceLocalAccountType for details";
  ScopedListPrefUpdate update(&local_state,
                              prefs::kDeviceLocalAccountsWithSavedData);
  update->Append(user_id);
}

// static
std::string TestHelper::GetFakeUsernameHash(const AccountId& account_id) {
  CHECK(account_id.is_valid());
  return ash::UserDataAuthClient::GetStubSanitizedUsername(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id));
}

TestHelper::TestHelper(UserManager* user_manager)
    : user_manager_(CHECK_DEREF(user_manager)) {}

TestHelper::~TestHelper() = default;

User* TestHelper::AddRegularUser(const AccountId& account_id) {
  return AddUserInternal(account_id, UserType::kRegular);
}

User* TestHelper::AddChildUser(const AccountId& account_id) {
  return AddUserInternal(account_id, UserType::kChild);
}

User* TestHelper::AddGuestUser() {
  return AddUserInternal(GuestAccountId(), UserType::kGuest);
}

User* TestHelper::AddUserInternal(const AccountId& account_id,
                                  UserType user_type) {
  if (user_manager_->FindUser(account_id)) {
    LOG(ERROR) << "User for " << account_id << " already exists";
    return nullptr;
  }

  if (!user_manager_->EnsureUser(account_id, user_type,
                                 /*is_ephemeral=*/false)) {
    LOG(ERROR) << "Failed to create a user " << user_type << " for "
               << account_id;
    return nullptr;
  }
  return user_manager_->FindUserAndModify(account_id);
}

User* TestHelper::AddKioskAppUser(std::string_view user_id) {
  // Quick check that the `user_id` satisfies kiosk-app type.
  auto type = policy::GetDeviceLocalAccountType(user_id);
  if (type != policy::DeviceLocalAccountType::kKioskApp) {
    LOG(ERROR)
        << "user_id (" << user_id << ") did not satisfy to be used for "
        << "a kiosk user. See policy::GetDeviceLocalAccountType for details.";
    return nullptr;
  }

  return AddDeviceLocalAccountUserInternal(user_id, UserType::kKioskApp);
}

User* TestHelper::AddWebKioskAppUser(std::string_view user_id) {
  // Quick check that the `user_id` satisfies web-kiosk-app type.
  auto type = policy::GetDeviceLocalAccountType(user_id);
  if (type != policy::DeviceLocalAccountType::kWebKioskApp) {
    LOG(ERROR) << "user_id (" << user_id << ") did not satisfy to be used for "
               << "a web kiosk user. See policy::GetDeviceLocalAccountType for "
                  "details.";
    return nullptr;
  }

  return AddDeviceLocalAccountUserInternal(user_id, UserType::kWebKioskApp);
}

User* TestHelper::AddPublicAccountUser(std::string_view user_id) {
  // Quick check that the `user_id` satisfies kiosk-app type.
  auto type = policy::GetDeviceLocalAccountType(user_id);
  if (type != policy::DeviceLocalAccountType::kPublicSession) {
    LOG(ERROR)
        << "user_id (" << user_id << ") did not satisfy to be used for "
        << "a public account user. See policy::GetDeviceLocalAccountType "
        << "for details.";
    return nullptr;
  }

  return AddDeviceLocalAccountUserInternal(user_id, UserType::kPublicAccount);
}

User* TestHelper::AddDeviceLocalAccountUserInternal(std::string_view user_id,
                                                    UserType user_type) {
  // Build DeviceLocalAccountInfo for the existing users.
  std::vector<UserManager::DeviceLocalAccountInfo> device_local_accounts;
  for (const auto& user : user_manager_->GetPersistedUsers()) {
    if (user->GetAccountId().GetUserEmail() == user_id) {
      LOG(ERROR) << "duplicated account is found: " << user_id;
      return nullptr;
    }
    if (!user->IsDeviceLocalAccount()) {
      continue;
    }
    UserManager::DeviceLocalAccountInfo info(
        user->GetAccountId().GetUserEmail(), user->GetType());
    if (user->GetType() == UserType::kPublicAccount) {
      info.display_name = user->GetDisplayName();
    }
    device_local_accounts.push_back(info);
  }

  // Add the given `user_id`.
  device_local_accounts.emplace_back(std::string(user_id), user_type);
  user_manager_->UpdateDeviceLocalAccountUser(device_local_accounts);
  return user_manager_->FindUserAndModify(AccountId::FromUserEmail(user_id));
}

}  // namespace user_manager
