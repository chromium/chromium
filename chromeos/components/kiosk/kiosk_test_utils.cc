// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kiosk/kiosk_test_utils.h"

#include "base/check.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"

namespace chromeos {

void SetUpFakeKioskSession(std::string_view email) {
  CHECK(user_manager::UserManager::IsInitialized())
      << "UserManager instance needs to be set up to start Kiosk session.";

  auto* user_manager = user_manager::UserManager::Get();
  auto* user = user_manager::TestHelper(user_manager).AddKioskAppUser(email);
  CHECK_EQ(user_manager->GetLoggedInUsers().size(), 0u);
  user_manager->UserLoggedIn(
      user->GetAccountId(),
      user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
  CHECK_EQ(user_manager->GetLoggedInUsers().size(), 1u);
  CHECK_EQ(user, user_manager->GetActiveUser());
}

}  // namespace chromeos
