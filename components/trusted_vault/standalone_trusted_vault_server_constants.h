// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_SERVER_CONSTANTS_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_SERVER_CONSTANTS_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "url/gurl.h"

namespace trusted_vault {

inline constexpr int kUnknownConstantKeyVersion = 0;

inline constexpr char kSecurityDomainPathPrefix[] = "users/me/securitydomains/";
inline constexpr char kSecurityDomainMemberNamePrefix[] = "users/me/members/";
inline constexpr char kJoinSecurityDomainsErrorDetailTypeURL[] =
    "type.googleapis.com/"
    "google.internal.identity.securitydomain.v1.JoinSecurityDomainErrorDetail";

inline constexpr char kQueryParameterAlternateOutputKey[] = "alt";
inline constexpr char kQueryParameterAlternateOutputProto[] = "proto";

std::vector<uint8_t> GetConstantTrustedVaultKey();
GURL GetGetSecurityDomainMembersURL(
    const GURL& server_url,
    const std::set<SecurityDomainId>& security_domain_filter,
    const std::set<trusted_vault_pb::SecurityDomainMember_MemberType>&
        member_filter);
GURL GetGetSecurityDomainMemberURL(const GURL& server_url,
                                   base::span<const uint8_t> public_key);
GURL GetGetSecurityDomainURL(const GURL& server_url,
                             SecurityDomainId security_domain);
GURL GetJoinSecurityDomainURL(const GURL& server_url,
                              SecurityDomainId security_domain);

// Computes full URL, including alternate proto param.
GURL GetGetSecurityDomainMembersURLForTesting(
    const std::optional<std::string>& next_page_token,
    const GURL& server_url,
    const std::set<SecurityDomainId>& security_domain_filter,
    const std::set<trusted_vault_pb::SecurityDomainMember_MemberType>&
        member_filter);
GURL GetFullJoinSecurityDomainsURLForTesting(const GURL& server_url,
                                             SecurityDomainId security_domain);
GURL GetFullGetSecurityDomainMemberURLForTesting(
    const GURL& server_url,
    base::span<const uint8_t> public_key);
GURL GetFullGetSecurityDomainURLForTesting(const GURL& server_url,
                                           SecurityDomainId security_domain);

std::string GetSecurityDomainPath(SecurityDomainId domain);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_SERVER_CONSTANTS_H_
