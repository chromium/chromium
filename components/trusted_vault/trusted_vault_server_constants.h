// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace trusted_vault {

inline constexpr char kSyncSecurityDomainName[] = "chromesync";
inline constexpr char kPasskeysSecurityDomainName[] = "hw_protected";

// Identifies a particular security domain.
//
// Append new values at the end and update kMaxValue. Note: Values are persisted
// in disk storage (e.g. DomainData.domain_id) and must not be renumbered or
// reused.
enum class SecurityDomainId {
  kChromeSync = 1,
  kPasskeys = 2,
  kMaxValue = kPasskeys,
};

inline constexpr auto kAllSecurityDomainIdValues =
    base::MakeFixedFlatSet<SecurityDomainId>(
        {SecurityDomainId::kChromeSync, SecurityDomainId::kPasskeys});
static_assert(static_cast<int>(SecurityDomainId::kMaxValue) ==
                  kAllSecurityDomainIdValues.size(),
              "Update kAllSecurityDomainIdValues when adding SecurityDomainId "
              "enum values");

std::optional<SecurityDomainId> GetSecurityDomainByName(
    std::string_view domain);
std::string_view GetSecurityDomainName(SecurityDomainId id);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
