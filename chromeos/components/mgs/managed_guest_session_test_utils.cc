// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mgs/managed_guest_session_test_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/login/login_state/login_state.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif

namespace chromeos {

FakeManagedGuestSession::FakeManagedGuestSession(bool initialize_login_state)
    : initialize_login_state_(initialize_login_state) {
  SetUpFakeManagedGuestSession();
}

FakeManagedGuestSession::~FakeManagedGuestSession() {
  TearDownFakeManagedGuestSession();
}

void FakeManagedGuestSession::SetUpFakeManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (initialize_login_state_) {
    ash::LoginState::Initialize();
  }
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_PUBLIC_ACCOUNT);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::BrowserInitParamsPtr init_params =
      chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
}

void FakeManagedGuestSession::TearDownFakeManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (initialize_login_state_) {
    ash::LoginState::Shutdown();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace chromeos
