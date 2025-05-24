// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_POLICY_UTIL_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_POLICY_UTIL_H_

#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"

namespace user_manager {

// Returns UserType corresponding to the given DeviceLocalAccountType.
USER_MANAGER_EXPORT UserType DeviceLocalAccountTypeToUserType(
    policy::DeviceLocalAccountType device_local_account_type);

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_POLICY_UTIL_H_
