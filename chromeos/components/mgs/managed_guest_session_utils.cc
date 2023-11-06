// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mgs/managed_guest_session_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/login/login_state/login_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos {

bool IsManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::LoginState::IsInitialized() &&
         ash::LoginState::Get()->IsManagedGuestSessionUser();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->SessionType() ==
         crosapi::mojom::SessionType::kPublicSession;
#else
  return false;
#endif
}

}  // namespace chromeos
