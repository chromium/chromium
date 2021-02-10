// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_

#include <vector>

#include "url/gurl.h"

namespace syncer {

extern const char kSyncSecurityDomainName[];
extern const char kJoinSecurityDomainsURLPath[];
extern const char kListSecurityDomainsURLPathAndQuery[];

std::vector<uint8_t> GetConstantTrustedVaultKey();

// Computes full URL, including alternate proto param.
GURL GetFullJoinSecurityDomainsURLForTesting(const GURL& server_url);
GURL GetFullListSecurityDomainsURLForTesting(const GURL& server_url);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SERVER_CONSTANTS_H_
