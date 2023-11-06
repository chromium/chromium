// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace trusted_vault {

inline constexpr int kUnknownConstantKeyVersion = 0;

inline constexpr char kSyncSecurityDomainName[] =
    "users/me/securitydomains/chromesync";
inline constexpr char kPasskeysSecurityDomainName[] =
    "users/me/securitydomains/hw_protected";
inline constexpr char kSecurityDomainMemberNamePrefix[] = "users/me/members/";
inline constexpr char kJoinSecurityDomainsErrorDetailTypeURL[] =
    "type.googleapis.com/"
    "google.internal.identity.securitydomain.v1.JoinSecurityDomainErrorDetail";

inline constexpr char kQueryParameterAlternateOutputKey[] = "alt";
inline constexpr char kQueryParameterAlternateOutputProto[] = "proto";

// Identifies a particular security domain.
//
// Append new values at the end and update kMaxValue. Values must not be
// persisted.
enum class SecurityDomainId {
  kChromeSync,
  kPasskeys,
  kMaxValue = kPasskeys,
};

inline constexpr auto kAllSecurityDomainIdValues =
    base::MakeFixedFlatSetSorted<SecurityDomainId>(
        {SecurityDomainId::kChromeSync, SecurityDomainId::kPasskeys});
static_assert(static_cast<int>(SecurityDomainId::kMaxValue) ==
                  kAllSecurityDomainIdValues.size() - 1,
              "Update kAllSecurityDomainIdValues when adding SecurityDomainId "
              "enum values");

std::vector<uint8_t> GetConstantTrustedVaultKey();
GURL GetGetSecurityDomainMemberURL(const GURL& server_url,
                                   base::span<const uint8_t> public_key);
GURL GetGetSecurityDomainURL(const GURL& server_url,
                             SecurityDomainId security_domain);
GURL GetJoinSecurityDomainURL(const GURL& server_url,
                              SecurityDomainId security_domain);

// Computes full URL, including alternate proto param.
GURL GetFullJoinSecurityDomainsURLForTesting(const GURL& server_url,
                                             SecurityDomainId security_domain);
GURL GetFullGetSecurityDomainMemberURLForTesting(
    const GURL& server_url,
    base::span<const uint8_t> public_key);
GURL GetFullGetSecurityDomainURLForTesting(const GURL& server_url,
                                           SecurityDomainId security_domain);

std::string GetSecurityDomainName(SecurityDomainId domain);
absl::optional<SecurityDomainId> GetSecurityDomainByName(
    base::StringPiece domain);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
