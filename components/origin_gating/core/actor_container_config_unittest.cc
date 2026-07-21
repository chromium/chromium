// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/actor_container_config.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_gating {

namespace {

using Location = ActorContainerConfig::Location;
using Rule = ActorContainerConfig::Rule;

Location WildcardLocation() {
  return Location(ActorContainerConfig::Wildcard());
}

Location SiteLocation(const GURL& url) {
  return Location(net::SchemefulSite(url));
}

Location OriginLocation(const GURL& url) {
  return Location(url::Origin::Create(url));
}

Rule CreateRule(std::vector<Location> navigation_sources = {},
                Rule::ResourceSet resources = {},
                Rule::CapabilitySet capabilities = {}) {
  return Rule(std::move(navigation_sources), resources, capabilities);
}

}  // namespace

class ActorContainerConfigTest : public testing::Test {
 public:
  ~ActorContainerConfigTest() override = default;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin kExampleDifferentSubdomainOrigin =
      url::Origin::Create(GURL("https://b.example.com"));
  const url::Origin kExampleInsecureOrigin =
      url::Origin::Create(GURL("http://a.example.com"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(GURL("https://b.foo.com"));

  const url::Origin kIgnoredOrigin =
      url::Origin::Create(GURL("https://ignoreme.com"));

  const url::Origin kWsOrigin = url::Origin::Create(GURL("ws://a.example.com"));
  const url::Origin kWssOrigin =
      url::Origin::Create(GURL("wss://a.example.com"));
  const url::Origin kCrossSiteWsOrigin =
      url::Origin::Create(GURL("ws://b.foo.com"));
  const url::Origin kCrossSiteWssOrigin =
      url::Origin::Create(GURL("wss://b.foo.com"));
};

TEST_F(ActorContainerConfigTest, EmptyConfigBlocksAll) {
  ActorContainerConfig config;

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, NoCapabilities) {
  ActorContainerConfig config({
      {WildcardLocation(), CreateRule({}, {Rule::Resource::kSession}, {})},
  });

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Wildcard_ActuationCapabilityAll) {
  ActorContainerConfig config({
      {WildcardLocation(),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Wildcard_WithSource) {
  ActorContainerConfig config({
      {WildcardLocation(),
       CreateRule({Location(kExampleOrigin)}, {Rule::Resource::kSession},
                  {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Site_NoCapabilities) {
  ActorContainerConfig config({
      {Location(net::SchemefulSite(kExampleOrigin)),
       CreateRule({}, {Rule::Resource::kSession}, {})},
  });

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Site_ActuationCapabilityAll) {
  ActorContainerConfig config({
      {Location(net::SchemefulSite(kExampleOrigin)),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, InsecureSite_ActuationCapabilityAll) {
  ActorContainerConfig config({
      {Location(net::SchemefulSite(kExampleInsecureOrigin)),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Site_WithSource) {
  ActorContainerConfig config({
      {Location(net::SchemefulSite(kExampleOrigin)),
       CreateRule({Location(kExampleOrigin)}, {Rule::Resource::kSession},
                  {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Origin_NoCapabilities) {
  ActorContainerConfig config({
      {Location(kExampleOrigin),
       CreateRule({}, {Rule::Resource::kSession}, {})},
  });

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Origin_ActuationCapabilityAll) {
  ActorContainerConfig config({
      {Location(kExampleOrigin),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, InsecureOrigin_ActuationCapabilityAll) {
  ActorContainerConfig config({
      {Location(kExampleInsecureOrigin),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest,
       OriginWithExplicitPort_ActuationCapabilityAll) {
  ActorContainerConfig config({
      {Location(url::Origin::Create(GURL("https://a.example.com:443"))),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Origin_WithSource) {
  ActorContainerConfig config({
      {Location(kExampleOrigin),
       CreateRule({Location(kExampleOrigin)}, {Rule::Resource::kSession},
                  {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, WildcardAndBlockedSite) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {})},
      {WildcardLocation(),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, BlockedWildcardAndAllowedSite) {
  ActorContainerConfig config({
      {WildcardLocation(), CreateRule({}, {Rule::Resource::kSession}, {})},
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, SiteAndBlockedOrigin) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
      {OriginLocation(GURL("https://b.example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, BlockedSiteAndOrigin) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {})},
      {Location(kExampleOrigin),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, NoCapability) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {})},
  });

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MultipleCapabilityAll) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, NoResources) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {}, {Rule::Capability::kAll})},
  });

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MultipleResourceSessions) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, WsOrigin) {
  ActorContainerConfig config({
      {Location(kWsOrigin),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Should not match https://.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Can only navigate https:// -> ws://.
  EXPECT_TRUE(config.IsActuationAllowed(kWsOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kWsOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kExampleOrigin));

  // Only navigation from wss:// -> ws:// allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kWssOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kWssOrigin, kWsOrigin));

  // Navigate to ws:// with different host should not be allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteWsOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kCrossSiteWsOrigin));
}

TEST_F(ActorContainerConfigTest, WssOrigin) {
  ActorContainerConfig config({
      {Location(kWssOrigin),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  // Should not match https://.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Can only navigate https:// -> wss://.
  EXPECT_FALSE(config.IsActuationAllowed(kWsOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kExampleOrigin));

  // Only navigation from ws:// -> wss:// allowed.
  EXPECT_TRUE(config.IsActuationAllowed(kWssOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kWsOrigin, kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWssOrigin, kWsOrigin));

  // Navigate to wss:// with different host should not be allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteWsOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWssOrigin, kCrossSiteWssOrigin));
}

TEST_F(ActorContainerConfigTest, ToDebugStringEmpty) {
  ActorContainerConfig config;

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(R"({"rules": {}})"));
}

TEST_F(ActorContainerConfigTest, ToDebugStringWildcard) {
  ActorContainerConfig config({
      {WildcardLocation(),
       CreateRule({}, {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(R"json({
        "rules": {
            "Wildcard": {
                "capabilities": ["CAPABILITY_ALL"],
                "accessible_resources": ["RESOURCE_SESSION"],
                "navigation_sources": []
            }
        }
    })json"));
}

TEST_F(ActorContainerConfigTest, ToDebugStringSiteWithNavigationSource) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({OriginLocation(GURL("https://a.example.com"))},
                  {Rule::Resource::kSession}, {Rule::Capability::kAll})},
  });

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(R"json({
        "rules": {
            "Site(https://example.com)": {
                "capabilities": ["CAPABILITY_ALL"],
                "accessible_resources": ["RESOURCE_SESSION"],
                "navigation_sources": ["Origin(https://a.example.com)"]
            }
        }
  })json"));
}

TEST_F(ActorContainerConfigTest, ToDebugStringMultipleRules) {
  ActorContainerConfig config({
      {SiteLocation(GURL("https://example.com")),
       CreateRule({OriginLocation(GURL("https://a.example.com"))},
                  {Rule::Resource::kSession}, {Rule::Capability::kAll})},
      {SiteLocation(GURL("https://foo.com")),
       CreateRule({OriginLocation(GURL("https://bar.example.com")),
                   SiteLocation(GURL("https://other.com"))},
                  {}, {Rule::Capability::kAll})},
  });

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(R"json({
        "rules": {
            "Site(https://example.com)": {
                "capabilities": ["CAPABILITY_ALL"],
                "accessible_resources": ["RESOURCE_SESSION"],
                "navigation_sources": ["Origin(https://a.example.com)"]
            },
            "Site(https://foo.com)": {
                "capabilities": ["CAPABILITY_ALL"],
                "accessible_resources": [],
                "navigation_sources": ["Origin(https://bar.example.com)",
                                      "Site(https://other.com)"]
            }
        }
  })json"));
}

}  // namespace origin_gating
