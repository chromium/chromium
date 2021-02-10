// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_server_constants.h"

#include "net/base/url_util.h"

namespace syncer {

namespace {

const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";

}  // namespace

const char kSyncSecurityDomainName[] = "chromesync";
const char kJoinSecurityDomainsURLPath[] = "/domain:join";
const char kListSecurityDomainsURLPathAndQuery[] = "/domain:list?view=1";

std::vector<uint8_t> GetConstantTrustedVaultKey() {
  return std::vector<uint8_t>(16, 0);
}

GURL GetFullJoinSecurityDomainsURLForTesting(const GURL& server_url) {
  return net::AppendQueryParameter(
      /*url=*/GURL(server_url.spec() + kJoinSecurityDomainsURLPath),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

GURL GetFullListSecurityDomainsURLForTesting(const GURL& server_url) {
  return net::AppendQueryParameter(
      /*url=*/GURL(server_url.spec() + kListSecurityDomainsURLPathAndQuery),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

}  // namespace syncer
