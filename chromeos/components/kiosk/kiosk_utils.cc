// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kiosk/kiosk_utils.h"

#include "components/user_manager/user_manager.h"

namespace chromeos {

bool IsKioskSession() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp();
}

bool IsWebKioskSession() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsKioskWebApp();
}

}  // namespace chromeos
