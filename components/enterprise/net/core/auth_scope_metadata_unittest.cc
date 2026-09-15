// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/auth_scope_metadata.h"

#include <optional>
#include <string_view>

#include "components/enterprise/net/core/types.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_net {

TEST(AuthScopeMetadataTest, GetAuthScopeMetadata_CloudSecureGateway) {
  std::optional<AuthScopeMetadata> metadata =
      GetAuthScopeMetadata(AuthScope::kCloudSecureGateway);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(signin::OAuthConsumerId::kSecureGatewayService,
            metadata->consumer_id);
  EXPECT_EQ(kAuthScopeCloudSecureGateway, metadata->policy_string);
  EXPECT_EQ(std::size(kCloudSecureGatewayAllowedDomains),
            metadata->allowed_domains.size());
}

TEST(AuthScopeMetadataTest, GetAuthScopeMetadata_Unsupported) {
  EXPECT_FALSE(GetAuthScopeMetadata(AuthScope::kNone).has_value());
  EXPECT_FALSE(GetAuthScopeMetadata(static_cast<AuthScope>(999)).has_value());
}

TEST(AuthScopeMetadataTest, GetOAuthConsumerIdForScope) {
  EXPECT_EQ(signin::OAuthConsumerId::kSecureGatewayService,
            GetOAuthConsumerIdForScope(AuthScope::kCloudSecureGateway));
  EXPECT_EQ(std::nullopt, GetOAuthConsumerIdForScope(AuthScope::kNone));
  EXPECT_EQ(std::nullopt,
            GetOAuthConsumerIdForScope(static_cast<AuthScope>(999)));
}

TEST(AuthScopeMetadataTest, IsDestinationAllowedForScope) {
  EXPECT_TRUE(
      IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway,
                                   GURL("https://gateway.securegateway.goog")));
  EXPECT_TRUE(IsDestinationAllowedForScope(
      AuthScope::kCloudSecureGateway,
      GURL("https://sub.gateway.securegateway.goog")));
  EXPECT_TRUE(IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway,
                                           GURL("https://securegateway.goog")));
  EXPECT_TRUE(
      IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway,
                                   GURL("https://securegateway.goog:443/foo")));

  // Non-HTTPS schemes must be rejected.
  EXPECT_FALSE(IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway,
                                            GURL("http://securegateway.goog")));
  EXPECT_FALSE(IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway,
                                            GURL("ws://securegateway.goog")));

  // Non-matching domains must be rejected.
  EXPECT_FALSE(IsDestinationAllowedForScope(
      AuthScope::kCloudSecureGateway, GURL("https://notsecuregateway.goog")));
  EXPECT_FALSE(IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway,
                                            GURL("https://example.com")));

  // Invalid URLs must be rejected.
  EXPECT_FALSE(
      IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway, GURL()));
  EXPECT_FALSE(IsDestinationAllowedForScope(AuthScope::kCloudSecureGateway,
                                            GURL("invalid-url")));

  // Unsupported scope must be rejected.
  EXPECT_FALSE(IsDestinationAllowedForScope(
      AuthScope::kNone, GURL("https://gateway.securegateway.goog")));
  EXPECT_FALSE(IsDestinationAllowedForScope(
      static_cast<AuthScope>(999), GURL("https://gateway.securegateway.goog")));
}

TEST(AuthScopeMetadataTest, AuthScopeToString) {
  EXPECT_EQ("cloud_secure_gateway",
            AuthScopeToString(AuthScope::kCloudSecureGateway));
  EXPECT_EQ(std::nullopt, AuthScopeToString(AuthScope::kNone));
  EXPECT_EQ(std::nullopt, AuthScopeToString(static_cast<AuthScope>(999)));
}

TEST(AuthScopeMetadataTest, ParseAuthScope) {
  EXPECT_EQ(AuthScope::kCloudSecureGateway,
            ParseAuthScope("cloud_secure_gateway"));
  EXPECT_EQ(AuthScope::kCloudSecureGateway,
            ParseAuthScope("CLOUD_SECURE_GATEWAY"));
  EXPECT_EQ(AuthScope::kCloudSecureGateway,
            ParseAuthScope("Cloud_Secure_Gateway"));
  EXPECT_EQ(AuthScope::kNone, ParseAuthScope("none"));
  EXPECT_EQ(AuthScope::kNone, ParseAuthScope("NONE"));
  EXPECT_EQ(AuthScope::kNone, ParseAuthScope("unknown_scope"));
  EXPECT_EQ(AuthScope::kNone, ParseAuthScope(""));
}

}  // namespace enterprise_net
