// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/device_local_account_type.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {
namespace {

constexpr auto kDomainPrefixMap =
    base::MakeFixedFlatMap<DeviceLocalAccountType, std::string_view>({
        {DeviceLocalAccountType::kPublicSession, "public-accounts"},
        {DeviceLocalAccountType::kKioskApp, "kiosk-apps"},
        {DeviceLocalAccountType::kSamlPublicSession, "saml-public-accounts"},
        {DeviceLocalAccountType::kWebKioskApp, "web-kiosk-apps"},
        {DeviceLocalAccountType::kKioskIsolatedWebApp, "isolated-kiosk-apps"},

    });

constexpr char kDeviceLocalAccountDomainSuffix[] = ".device-local.localhost";

}  // namespace

bool IsValidDeviceLocalAccountType(int value) {
  switch (static_cast<DeviceLocalAccountType>(value)) {
    case DeviceLocalAccountType::kPublicSession:
    case DeviceLocalAccountType::kKioskApp:
    case DeviceLocalAccountType::kSamlPublicSession:
    case DeviceLocalAccountType::kWebKioskApp:
    case DeviceLocalAccountType::kKioskIsolatedWebApp:
      return true;
  }
  return false;
}

std::string GenerateDeviceLocalAccountUserId(std::string_view account_id,
                                             DeviceLocalAccountType type) {
  const auto it = kDomainPrefixMap.find(type);
  CHECK(it != kDomainPrefixMap.end());
  return gaia::CanonicalizeEmail(
      base::StrCat({base::HexEncode(account_id), "@", it->second,
                    kDeviceLocalAccountDomainSuffix}));
}

base::expected<DeviceLocalAccountType, GetDeviceLocalAccountTypeError>
GetDeviceLocalAccountType(std::string_view user_id) {
  // For historical reasons, the guest user ID does not contain an @ symbol and
  // therefore, cannot be parsed by gaia::ExtractDomainName().
  if (user_id.find('@') == std::string_view::npos) {
    return base::unexpected(
        GetDeviceLocalAccountTypeError::kNoDeviceLocalAccountUser);
  }

  const std::string domain = gaia::ExtractDomainName(user_id);
  if (!base::EndsWith(domain, kDeviceLocalAccountDomainSuffix,
                      base::CompareCase::SENSITIVE)) {
    return base::unexpected(
        GetDeviceLocalAccountTypeError::kNoDeviceLocalAccountUser);
  }

  // Strip the domain suffix.
  std::string_view domain_prefix = domain;
  domain_prefix.remove_suffix(sizeof(kDeviceLocalAccountDomainSuffix) - 1);

  // Reverse look up from the map.
  for (const auto& [type, candidate] : kDomainPrefixMap) {
    if (domain_prefix == candidate) {
      return base::ok(type);
    }
  }

  // |user_id| is a device-local account but its type is not recognized.
  DUMP_WILL_BE_NOTREACHED();
  return base::unexpected(GetDeviceLocalAccountTypeError::kUnknownDomain);
}

bool IsDeviceLocalAccountUser(std::string_view user_id) {
  return GetDeviceLocalAccountType(user_id) !=
         base::unexpected(
             GetDeviceLocalAccountTypeError::kNoDeviceLocalAccountUser);
}

}  // namespace policy
