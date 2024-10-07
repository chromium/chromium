// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kiosk/kiosk_test_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/check.h"
#include "components/account_id/account_id.h"  // nogncheck
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"  // nogncheck
#endif

namespace chromeos {

void SetUpFakeKioskSession(const std::string& email) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::BrowserInitParamsPtr init_params =
      chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->session_type = crosapi::mojom::SessionType::kWebKioskSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
}

}  // namespace chromeos
