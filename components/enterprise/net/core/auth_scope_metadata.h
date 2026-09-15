// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_AUTH_SCOPE_METADATA_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_AUTH_SCOPE_METADATA_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "components/enterprise/net/core/types.h"
#include "components/signin/public/base/oauth_consumer_id.h"

class GURL;

namespace enterprise_net {

// Allowed domains for Cloud Secure Gateway OAuth tokens.
inline constexpr std::string_view kCloudSecureGatewayAllowedDomains[] = {
    "securegateway.goog",
};

// String representation in policies and configs for Cloud Secure Gateway.
inline constexpr std::string_view kAuthScopeCloudSecureGateway =
    "cloud_secure_gateway";

// Mapping of AuthScope to its metadata, including the OAuthConsumerId, its
// policy string representation, and the set of destination domains to which
// tokens for this scope may be sent.
struct AuthScopeMetadata {
  signin::OAuthConsumerId consumer_id;
  std::string_view policy_string;
  base::raw_span<const std::string_view> allowed_domains;
};

// Returns metadata for the given `scope`, or std::nullopt if the scope is
// unsupported (e.g. AuthScope::kNone).
std::optional<AuthScopeMetadata> GetAuthScopeMetadata(AuthScope scope);

// Returns the OAuthConsumerId associated with `scope`, or std::nullopt if the
// scope is unsupported.
std::optional<signin::OAuthConsumerId> GetOAuthConsumerIdForScope(
    AuthScope scope);

// Returns true if `destination_url` is allowed to receive an access token for
// `scope`. Validates that the URL uses the HTTPS scheme and matches an allowed
// domain for `scope`.
bool IsDestinationAllowedForScope(AuthScope scope, const GURL& destination_url);

// Converts an AuthScope to its string representation used in policies and
// preferences. Returns std::nullopt for AuthScope::kNone or unrecognized
// scopes.
std::optional<std::string_view> AuthScopeToString(AuthScope scope);

// Parses a policy or preference string into an AuthScope. Returns
// AuthScope::kNone if unrecognized.
AuthScope ParseAuthScope(std::string_view scope_str);

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_AUTH_SCOPE_METADATA_H_
