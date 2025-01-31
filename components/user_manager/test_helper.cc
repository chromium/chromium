// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/test_helper.h"

#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"

namespace user_manager {

// static
void TestHelper::RegisterPersistedUser(PrefService& local_state,
                                       const AccountId& account_id) {
  {
    ScopedListPrefUpdate update(&local_state, prefs::kRegularUsersPref);
    update->Append(account_id.GetUserEmail());
  }
  {
    KnownUser known_user(&local_state);
    known_user.UpdateId(account_id);
  }
}

TestHelper::TestHelper(UserManager& user_manager)
    : user_manager_(user_manager) {}

TestHelper::~TestHelper() = default;

User* TestHelper::AddKioskAppUser(std::string_view user_id) {
  // Quick check that the `user_id` satisfies kiosk-app type.
  auto type = policy::GetDeviceLocalAccountType(user_id);
  if (type != policy::DeviceLocalAccountType::kKioskApp) {
    LOG(ERROR)
        << "user_id (" << user_id << ") did not satisfy to be used for "
        << "a kiosk user. See policy::GetDeviceLocalAccountType for details.";
    return nullptr;
  }

  // Build DeviceLocalAccountInfo for the existing users.
  std::vector<UserManager::DeviceLocalAccountInfo> device_local_accounts;
  for (const auto& user : user_manager_->GetUsers()) {
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
  device_local_accounts.emplace_back(std::string(user_id), UserType::kKioskApp);
  user_manager_->UpdateDeviceLocalAccountUser(device_local_accounts);
  return user_manager_->FindUserAndModify(AccountId::FromUserEmail(user_id));
}

}  // namespace user_manager
