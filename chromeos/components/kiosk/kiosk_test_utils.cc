// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kiosk/kiosk_test_utils.h"

#include "base/check.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"

namespace chromeos {

void SetUpFakeKioskSession(const std::string& email) {
  CHECK(user_manager::UserManager::Get());

  auto* user_manager = static_cast<user_manager::UserManagerImpl*>(
      user_manager::UserManager::Get());
  auto account_id = AccountId::FromUserEmail(email);
  auto* user = user_manager->AddKioskAppUserForTesting(
      account_id,
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /* browser_restart= */ false,
                             /* is_child= */ false);
}

}  // namespace chromeos
