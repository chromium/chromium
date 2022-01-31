// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "url/gurl.h"

namespace syncer {

constexpr inline int kUnknownConstantKeyVersion = 0;

constexpr inline char kSyncSecurityDomainName[] =
    "users/me/securitydomains/chromesync";
constexpr inline char kSecurityDomainMemberNamePrefix[] = "users/me/members/";
constexpr inline char kJoinSecurityDomainsURLPath[] =
    "users/me/securitydomains/chromesync:join";
constexpr inline char kJoinSecurityDomainsErrorDetailTypeURL[] =
    "type.googleapis.com/"
    "google.internal.identity.securitydomain.v1.JoinSecurityDomainErrorDetail";
constexpr inline char kGetSecurityDomainURLPathAndQuery[] =
    "users/me/securitydomains/chromesync?view=2";

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
