// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kiosk/kiosk_test_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/login/login_state/login_state.h"  // nogncheck
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"  // nogncheck
#endif

namespace chromeos {

void SetUpFakeKioskSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::LoginState::Initialize();
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::BrowserInitParamsPtr init_params =
      chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->session_type = crosapi::mojom::SessionType::kWebKioskSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
}

void TearDownFakeKioskSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::LoginState::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace chromeos
