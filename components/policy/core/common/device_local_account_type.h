// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_DEVICE_LOCAL_ACCOUNT_TYPE_H_
#define COMPONENTS_POLICY_CORE_COMMON_DEVICE_LOCAL_ACCOUNT_TYPE_H_

#include <string>

#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "components/policy/policy_export.h"

namespace policy {

// This must match with DeviceLocalAccountInfoProto.AccountType in
// chrome_device_policy.proto
enum class DeviceLocalAccountType {
  // A login-less, policy-configured browsing session.
  kPublicSession,

  // An account that serves as a container for a single full-screen app.
  kKioskApp,

  // An account that serves as a container for a single full-screen
  // Android app.
  kArcKioskApp,

  // SAML public session account
  kSamlPublicSession,

  // An account that serves as a container for a single full-screen web app.
  kWebKioskApp,
};

POLICY_EXPORT std::string GenerateDeviceLocalAccountUserId(
    base::StringPiece account_id,
    DeviceLocalAccountType type);

enum class GetDeviceLocalAccountTypeError {
  kNoDeviceLocalAccountUser,
  kUnknownDomain,
};

// Returns the type of device-local account.
POLICY_EXPORT
base::expected<DeviceLocalAccountType, GetDeviceLocalAccountTypeError>
GetDeviceLocalAccountType(base::StringPiece user_id);

// Returns whether |user_id| belongs to a device-local account.
// This is equivalent to that GetDeviceLocalAccountType does not return
// kNoDeviceLocalAccountUser error.
POLICY_EXPORT bool IsDeviceLocalAccountUser(base::StringPiece user_id);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_DEVICE_LOCAL_ACCOUNT_TYPE_H_
