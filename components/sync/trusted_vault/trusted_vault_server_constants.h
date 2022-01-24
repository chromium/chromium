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

extern const int kUnknownConstantKeyVersion;

extern const char kSyncSecurityDomainName[];
extern const char kSecurityDomainMemberNamePrefix[];
extern const char kJoinSecurityDomainsURLPath[];
extern const char kJoinSecurityDomainsErrorDetailTypeURL[];
extern const char kGetSecurityDomainURLPathAndQuery[];

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
