// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager_policy_util.h"

#include "base/notreached.h"

namespace user_manager {

UserType DeviceLocalAccountTypeToUserType(
    policy::DeviceLocalAccountType device_local_account_type) {
  switch (device_local_account_type) {
    case policy::DeviceLocalAccountType::kPublicSession:
      return UserType::kPublicAccount;
    case policy::DeviceLocalAccountType::kSamlPublicSession:
      // TODO(b/345700258): Unused in the production. Remove the case.
      NOTREACHED();
    case policy::DeviceLocalAccountType::kKioskApp:
      return UserType::kKioskApp;
    case policy::DeviceLocalAccountType::kWebKioskApp:
      return UserType::kWebKioskApp;
    case policy::DeviceLocalAccountType::kKioskIsolatedWebApp:
      return UserType::kKioskIWA;
    // TODO(crbug.com/388602323): Create new user type for ARCVM Kiosk.
    case policy::DeviceLocalAccountType::kArcvmKioskApp:
      return UserType::kKioskIWA;
  }
}

}  // namespace user_manager
