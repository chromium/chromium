// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "url/gurl.h"

namespace syncer {

inline constexpr int kUnknownConstantKeyVersion = 0;

inline constexpr char kSyncSecurityDomainName[] =
    "users/me/securitydomains/chromesync";
inline constexpr char kSecurityDomainMemberNamePrefix[] = "users/me/members/";
inline constexpr char kJoinSecurityDomainsURLPath[] =
    "users/me/securitydomains/chromesync:join";
inline constexpr char kJoinSecurityDomainsErrorDetailTypeURL[] =
    "type.googleapis.com/"
    "google.internal.identity.securitydomain.v1.JoinSecurityDomainErrorDetail";
inline constexpr char kGetSecurityDomainURLPathAndQuery[] =
    "users/me/securitydomains/chromesync?view=2";

inline constexpr char kQueryParameterAlternateOutputKey[] = "alt";
inline constexpr char kQueryParameterAlternateOutputProto[] = "proto";

std::vector<uint8_t> GetConstantTrustedVaultKey();
std::string GetGetSecurityDomainMemberURLPathAndQuery(
    base::span<const uint8_t> public_key);

// Computes full URL, including alternate proto param.
GURL GetFullJoinSecurityDomainsURLForTesting(const GURL& server_url);
GURL GetFullGetSecurityDomainMemberURLForTesting(
    const GURL& server_url,
    base::span<const uint8_t> public_key);
GURL GetFullGetSecurityDomainURLForTesting(const GURL& server_url);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
