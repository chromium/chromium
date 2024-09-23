// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_DEVICE_LOCAL_ACCOUNT_TYPE_H_
#define COMPONENTS_POLICY_CORE_COMMON_DEVICE_LOCAL_ACCOUNT_TYPE_H_

#include <string>
#include <string_view>

#include "base/types/expected.h"
#include "components/policy/policy_export.h"

namespace policy {

// This must match with DeviceLocalAccountInfoProto.AccountType in
// chrome_device_policy.proto
enum class DeviceLocalAccountType {
  // A login-less, policy-configured browsing session.
  kPublicSession = 0,

  // An account that serves as a container for a single full-screen ChromeApp.
  kKioskApp = 1,

  // An account that serves as a container for a single full-screen Android
  // app.
  // kArcKioskApp = 2, deprecated

  // SAML public session account
  kSamlPublicSession = 3,

  // An account that serves as a container for a single full-screen web app.
  kWebKioskApp = 4,

  // An account that serves as a container for a single full-screen
  // Isolated Web App (IWA).
  kKioskIsolatedWebApp = 5,
};

// Returns whether the given value is valid DeviceLocalAccountType.
POLICY_EXPORT bool IsValidDeviceLocalAccountType(int value);

POLICY_EXPORT std::string GenerateDeviceLocalAccountUserId(
    std::string_view account_id,
    DeviceLocalAccountType type);

enum class GetDeviceLocalAccountTypeError {
  kNoDeviceLocalAccountUser,
  kUnknownDomain,
};

// Returns the type of device-local account.
POLICY_EXPORT
base::expected<DeviceLocalAccountType, GetDeviceLocalAccountTypeError>
GetDeviceLocalAccountType(std::string_view user_id);

// Returns whether |user_id| belongs to a device-local account.
// This is equivalent to that GetDeviceLocalAccountType does not return
// kNoDeviceLocalAccountUser error.
POLICY_EXPORT bool IsDeviceLocalAccountUser(std::string_view user_id);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_DEVICE_LOCAL_ACCOUNT_TYPE_H_
