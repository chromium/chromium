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

namespace {

enum class KioskType {
  kChromeAppKiosk = 0,
  kWebKiosk = 1,
  kIwaKiosk = 2,
};

void AddKioskUser(std::string_view email, KioskType kiosk_type) {
  CHECK(user_manager::UserManager::IsInitialized())
      << "UserManager instance needs to be set up to start Kiosk session.";
  auto* user_manager = user_manager::UserManager::Get();

  user_manager::User* user;
  switch (kiosk_type) {
    case KioskType::kChromeAppKiosk:
      user =
          user_manager::TestHelper(user_manager).AddKioskChromeAppUser(email);
      break;
    case KioskType::kWebKiosk:
      user = user_manager::TestHelper(user_manager).AddKioskWebAppUser(email);
      break;
    case KioskType::kIwaKiosk:
      user = user_manager::TestHelper(user_manager).AddKioskIwaUser(email);
      break;
  }

  CHECK_EQ(user_manager->GetLoggedInUsers().size(), 0u);
  user_manager->UserLoggedIn(
      user->GetAccountId(),
      user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
  CHECK_EQ(user_manager->GetLoggedInUsers().size(), 1u);
  CHECK_EQ(user, user_manager->GetActiveUser());
}

}  // namespace

void SetUpFakeChromeAppKioskSession(std::string_view email) {
  AddKioskUser(email, KioskType::kChromeAppKiosk);
}

void SetUpFakeWebKioskSession(std::string_view email) {
  AddKioskUser(email, KioskType::kWebKiosk);
}

void SetUpFakeIwaKioskSession(std::string_view email) {
  AddKioskUser(email, KioskType::kIwaKiosk);
}

}  // namespace chromeos
