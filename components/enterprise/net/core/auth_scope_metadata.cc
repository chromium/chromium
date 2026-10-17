// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/auth_scope_metadata.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/enterprise/net/core/features.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace enterprise_net {

std::optional<AuthScopeMetadata> GetAuthScopeMetadata(AuthScope scope) {
  switch (scope) {
    case AuthScope::kCloudSecureGateway:
      return AuthScopeMetadata{
          .consumer_id = signin::OAuthConsumerId::kSecureGatewayService,
          .policy_string = kAuthScopeCloudSecureGateway,
          .allowed_domains = kCloudSecureGatewayAllowedDomains,
      };
    case AuthScope::kNone:
      return std::nullopt;
  }
  return std::nullopt;
}

std::optional<signin::OAuthConsumerId> GetOAuthConsumerIdForScope(
    AuthScope scope) {
  std::optional<AuthScopeMetadata> metadata = GetAuthScopeMetadata(scope);
  return metadata ? std::make_optional(metadata->consumer_id) : std::nullopt;
}

namespace {

std::vector<std::string>& GetExtraAllowedDomainsStorage() {
  static base::NoDestructor<std::vector<std::string>> domains;
  return *domains;
}

// Returns true if `destination_url` is allowed for `scope` by custom domain
// patterns configured in `kAllowedAuthDomainsParam` when dynamic route fetching
// is enabled.
bool IsDestinationAllowedByFeatureParam(AuthScope scope,
                                        const GURL& destination_url) {
  if (!destination_url.is_valid() || !GetAuthScopeMetadata(scope).has_value() ||
      !enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return false;
  }

  const std::string& param_val = kAllowedAuthDomainsParam.Get();
  if (param_val.empty()) {
    return false;
  }

  net::SchemeHostPortMatcher extra_matcher;
  for (std::string_view pattern : base::SplitStringPiece(
           param_val, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::unique_ptr<net::SchemeHostPortMatcherRule> rule =
        net::SchemeHostPortMatcherRule::FromUntrimmedRawString(pattern);
    if (rule) {
      extra_matcher.AddAsLastRule(std::move(rule));
    }
  }
  return extra_matcher.Includes(destination_url);
}

}  // namespace

void SetExtraAllowedDomainsForTesting(std::vector<std::string> extra_domains) {
  GetExtraAllowedDomainsStorage() = std::move(extra_domains);
}

const std::vector<std::string>& GetExtraAllowedDomainsForTesting() {
  return GetExtraAllowedDomainsStorage();
}

bool IsDestinationAllowedForScope(AuthScope scope,
                                  const GURL& destination_url,
                                  version_info::Channel channel) {
  if (!destination_url.is_valid()) {
    return false;
  }

  for (const std::string& domain : GetExtraAllowedDomainsStorage()) {
    if (destination_url.DomainIs(domain)) {
      return true;
    }
  }

  std::optional<AuthScopeMetadata> metadata = GetAuthScopeMetadata(scope);
  if (!metadata) {
    return false;
  }

  if (destination_url.SchemeIs(url::kHttpsScheme)) {
    for (std::string_view domain : metadata->allowed_domains) {
      if (destination_url.DomainIs(domain)) {
        return true;
      }
    }
  }

  // Custom auth domains configured via feature param are only permitted on
  // non-stable channels (Canary, Dev, and local/Unknown builds) for developer
  // testing.
  switch (channel) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      return IsDestinationAllowedByFeatureParam(scope, destination_url);
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      return false;
  }
}

std::optional<std::string_view> AuthScopeToString(AuthScope scope) {
  std::optional<AuthScopeMetadata> metadata = GetAuthScopeMetadata(scope);
  return metadata ? std::make_optional(metadata->policy_string) : std::nullopt;
}

AuthScope ParseAuthScope(std::string_view scope_str) {
  std::string normalized = base::ToLowerASCII(scope_str);
  static constexpr auto kAuthScopeMap =
      base::MakeFixedFlatMap<std::string_view, AuthScope>({
          {kAuthScopeCloudSecureGateway, AuthScope::kCloudSecureGateway},
      });
  auto it = kAuthScopeMap.find(normalized);
  return it != kAuthScopeMap.end() ? it->second : AuthScope::kNone;
}

}  // namespace enterprise_net
