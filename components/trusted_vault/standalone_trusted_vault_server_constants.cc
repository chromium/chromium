// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"

#include <string_view>

#include "base/base64url.h"
#include "base/containers/fixed_flat_map.h"
#include "net/base/url_util.h"

namespace trusted_vault {

std::vector<uint8_t> GetConstantTrustedVaultKey() {
  return std::vector<uint8_t>(16, 0);
}

GURL GetGetSecurityDomainMembersURL(
    const GURL& server_url,
    const std::set<SecurityDomainId>& security_domain_filter,
    const std::set<trusted_vault_pb::SecurityDomainMember_MemberType>&
        member_filter) {
  // View three is `SECURITY_DOMAIN_MEMBER_METADATA`.
  GURL request_url =
      GURL(server_url.spec() + kSecurityDomainMemberNamePrefix + "?view=3");

  for (const auto& security_domain : security_domain_filter) {
    request_url =
        net::AppendQueryParameter(request_url, "include_security_domains",
                                  GetSecurityDomainPath(security_domain));
  }
  for (const auto& member_type : member_filter) {
    request_url = net::AppendQueryParameter(request_url, "include_member_types",
                                            base::NumberToString(member_type));
  }

  return request_url;
}

GURL GetGetSecurityDomainMemberURL(const GURL& server_url,
                                   base::span<const uint8_t> public_key) {
  std::string encoded_public_key;
  base::Base64UrlEncode(std::string(public_key.begin(), public_key.end()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);
  return GURL(server_url.spec() + kSecurityDomainMemberNamePrefix +
              encoded_public_key + "?view=2" +
              "&request_header.force_master_read=true");
}

GURL GetGetSecurityDomainURL(const GURL& server_url,
                             SecurityDomainId security_domain) {
  return GURL(server_url.spec() + GetSecurityDomainPath(security_domain) +
              "?view=2");
}

GURL GetJoinSecurityDomainURL(const GURL& server_url,
                              SecurityDomainId security_domain) {
  return GURL(server_url.spec() + GetSecurityDomainPath(security_domain) +
              ":join");
}

GURL GetGetSecurityDomainMembersURLForTesting(
    const std::optional<std::string>& next_page_token,
    const GURL& server_url,
    const std::set<SecurityDomainId>& security_domain_filter,
    const std::set<trusted_vault_pb::SecurityDomainMember_MemberType>&
        member_filter) {
  GURL url = GetGetSecurityDomainMembersURL(server_url, security_domain_filter,
                                            member_filter);
  if (next_page_token) {
    url = net::AppendQueryParameter(url, "page_token", *next_page_token);
  }
  return net::AppendQueryParameter(url, kQueryParameterAlternateOutputKey,
                                   kQueryParameterAlternateOutputProto);
}

GURL GetFullJoinSecurityDomainsURLForTesting(const GURL& server_url,
                                             SecurityDomainId security_domain) {
  return net::AppendQueryParameter(
      GetJoinSecurityDomainURL(server_url, security_domain),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

GURL GetFullGetSecurityDomainMemberURLForTesting(
    const GURL& server_url,
    base::span<const uint8_t> public_key) {
  return net::AppendQueryParameter(
      GetGetSecurityDomainMemberURL(server_url, public_key),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

GURL GetFullGetSecurityDomainURLForTesting(const GURL& server_url,
                                           SecurityDomainId security_domain) {
  return net::AppendQueryParameter(
      GetGetSecurityDomainURL(server_url, security_domain),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

std::string GetSecurityDomainPath(SecurityDomainId domain) {
  switch (domain) {
    case SecurityDomainId::kChromeSync:
      return std::string(kSecurityDomainPathPrefix) + kSyncSecurityDomainName;
    case SecurityDomainId::kPasskeys:
      return std::string(kSecurityDomainPathPrefix) +
             kPasskeysSecurityDomainName;
  }
}

}  // namespace trusted_vault
