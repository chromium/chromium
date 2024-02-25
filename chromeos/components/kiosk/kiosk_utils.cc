// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kiosk/kiosk_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos {

bool IsKioskSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::SessionType session_type =
      chromeos::BrowserParamsProxy::Get()->SessionType();
  return session_type == crosapi::mojom::SessionType::kWebKioskSession ||
         session_type == crosapi::mojom::SessionType::kAppKioskSession;
#else
  return false;
#endif
}

bool IsWebKioskSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::SessionType session_type =
      chromeos::BrowserParamsProxy::Get()->SessionType();
  return session_type == crosapi::mojom::SessionType::kWebKioskSession;
#else
  return false;
#endif
}

}  // namespace chromeos
