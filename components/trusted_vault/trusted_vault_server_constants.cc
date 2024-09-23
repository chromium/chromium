// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_server_constants.h"

#include <string_view>

#include "base/base64url.h"
#include "base/containers/fixed_flat_map.h"
#include "net/base/url_util.h"

namespace trusted_vault {

std::vector<uint8_t> GetConstantTrustedVaultKey() {
  return std::vector<uint8_t>(16, 0);
}

GURL GetGetSecurityDomainMembersURL(const GURL& server_url) {
  // View three is `SECURITY_DOMAIN_MEMBER_METADATA`.
  return GURL(server_url.spec() + kSecurityDomainMemberNamePrefix + "?view=3");
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
    const GURL& server_url) {
  GURL url = GetGetSecurityDomainMembersURL(server_url);
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
