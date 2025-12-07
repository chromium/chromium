// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_server_constants.h"

#include "base/containers/fixed_flat_map.h"

namespace trusted_vault {

std::optional<SecurityDomainId> GetSecurityDomainByName(std::string_view name) {
  static_assert(static_cast<int>(SecurityDomainId::kMaxValue) == 1,
                "Update GetSecurityDomainByName and its unit tests when adding "
                "SecurityDomainId enum values");
  static constexpr auto kSecurityDomainNames =
      base::MakeFixedFlatMap<std::string_view, SecurityDomainId>({
          {kSyncSecurityDomainName, SecurityDomainId::kChromeSync},
          {kPasskeysSecurityDomainName, SecurityDomainId::kPasskeys},
      });
  return kSecurityDomainNames.contains(name)
             ? std::make_optional(kSecurityDomainNames.at(name))
             : std::nullopt;
}

std::string_view GetSecurityDomainName(SecurityDomainId id) {
  switch (id) {
    case SecurityDomainId::kChromeSync:
      return kSyncSecurityDomainName;
    case SecurityDomainId::kPasskeys:
      return kPasskeysSecurityDomainName;
  }
}

}  // namespace trusted_vault
