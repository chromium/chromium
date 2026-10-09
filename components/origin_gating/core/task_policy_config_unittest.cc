// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/task_policy_config.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_gating {

namespace {

using Location = TaskPolicyConfig::Location;
using Rule = TaskPolicyConfig::Rule;

Location WildcardLocation() {
  return Location(TaskPolicyConfig::Wildcard());
}

Location SiteLocation(const GURL& url) {
  return Location(net::SchemefulSite(url));
}

Location OriginLocation(const GURL& url) {
  return Location(url::Origin::Create(url));
}

enum class TestTool {
  kClick = 1,
  kType = 2,
  kNavigate = 3,
};

enum class OtherTestTool {
  kOther = 1,
};

Rule CreateRule(absl::flat_hash_set<Location> navigation_sources = {},
                Rule::ResourceSet resources = {},
                absl::flat_hash_set<ClientTool> allowed_tools = {}) {
  return Rule(std::move(navigation_sources), resources,
              std::move(allowed_tools));
}

// Substitutes each `$1` in `json_template` with the JSON serialization of
// `tool.ToDebugValue()`, which contains an address that varies between runs.
std::string ExpectedJsonWithTool(std::string_view json_template,
                                 const ClientTool& tool) {
  return base::ReplaceStringPlaceholders(
      json_template, {base::WriteJson(tool.ToDebugValue()).value()}, nullptr);
}

}  // namespace

template <>
const ToolDomain ToolDomain::kInstance<TestTool>{};

template <>
const ToolDomain ToolDomain::kInstance<OtherTestTool>{};

class TaskPolicyConfigTest : public testing::Test {
 public:
  ~TaskPolicyConfigTest() override = default;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin kExampleDifferentSubdomainOrigin =
      url::Origin::Create(GURL("https://b.example.com"));
  const net::SchemefulSite kExampleSite{kExampleOrigin};
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

TEST_F(TaskPolicyConfigTest, EmptyConfigBlocksAll) {
  TaskPolicyConfig config;

  // Same-site.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, NoCapabilities) {
  TaskPolicyConfig config(
      {
          {WildcardLocation(), CreateRule({}, {Rule::Resource::kSession}, {})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Wildcard_ActuationCapabilityAll) {
  TaskPolicyConfig config(
      {
          {WildcardLocation(), CreateRule({}, {Rule::Resource::kSession},
                                          {ClientTool(TestTool::kClick),
                                           ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Wildcard_WithSource) {
  TaskPolicyConfig config(
      {
          {WildcardLocation(),
           CreateRule({Location(kExampleOrigin)}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Site_NoCapabilities) {
  TaskPolicyConfig config(
      {
          {Location(net::SchemefulSite(kExampleOrigin)),
           CreateRule({}, {Rule::Resource::kSession}, {})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Site_ActuationCapabilityAll) {
  TaskPolicyConfig config(
      {
          {Location(net::SchemefulSite(kExampleOrigin)),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, InsecureSite_ActuationCapabilityAll) {
  TaskPolicyConfig config(
      {
          {Location(net::SchemefulSite(kExampleInsecureOrigin)),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Site_WithSource) {
  TaskPolicyConfig config(
      {
          {Location(net::SchemefulSite(kExampleOrigin)),
           CreateRule({Location(kExampleOrigin)}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Origin_NoCapabilities) {
  TaskPolicyConfig config(
      {
          {Location(kExampleOrigin),
           CreateRule({}, {Rule::Resource::kSession}, {})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Origin_ActuationCapabilityAll) {
  TaskPolicyConfig config(
      {
          {Location(kExampleOrigin),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, InsecureOrigin_ActuationCapabilityAll) {
  TaskPolicyConfig config(
      {
          {Location(kExampleInsecureOrigin),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, OriginWithExplicitPort_ActuationCapabilityAll) {
  TaskPolicyConfig config(
      {
          {Location(url::Origin::Create(GURL("https://a.example.com:443"))),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, Origin_WithSource) {
  TaskPolicyConfig config(
      {
          {Location(kExampleOrigin),
           CreateRule({Location(kExampleOrigin)}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, WildcardAndBlockedSite) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {Rule::Resource::kSession}, {})},
          {WildcardLocation(), CreateRule({}, {Rule::Resource::kSession},
                                          {ClientTool(TestTool::kClick),
                                           ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, BlockedWildcardAndAllowedSite) {
  TaskPolicyConfig config(
      {
          {WildcardLocation(), CreateRule({}, {Rule::Resource::kSession}, {})},
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, SiteAndBlockedOrigin) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
          {OriginLocation(GURL("https://b.example.com")),
           CreateRule({}, {Rule::Resource::kSession}, {})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, BlockedSiteAndOrigin) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {Rule::Resource::kSession}, {})},
          {Location(kExampleOrigin),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Same-site.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, NoCapability) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {Rule::Resource::kSession}, {})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, MultipleCapabilityAll) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, NoResources) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, MultipleResourceSessions) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick),
                       ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, WsOrigin) {
  TaskPolicyConfig config(
      {
          {Location(kWsOrigin), CreateRule({}, {Rule::Resource::kSession},
                                           {ClientTool(TestTool::kClick),
                                            ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Should not match https://.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Can only navigate https:// -> ws://.
  EXPECT_TRUE(
      config.IsActuationAllowed(kWsOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kWsOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kExampleOrigin));

  // Only navigation from wss:// -> ws:// allowed.
  EXPECT_FALSE(
      config.IsActuationAllowed(kWssOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kWssOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kWssOrigin, kWsOrigin));

  // Navigate to ws:// with different host should not be allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteWsOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kCrossSiteWsOrigin));
}

TEST_F(TaskPolicyConfigTest, WssOrigin) {
  TaskPolicyConfig config(
      {
          {Location(kWssOrigin), CreateRule({}, {Rule::Resource::kSession},
                                            {ClientTool(TestTool::kClick),
                                             ClientTool(TestTool::kNavigate)})},
      },
      ClientTool(TestTool::kNavigate));

  // Should not match https://.
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Can only navigate https:// -> wss://.
  EXPECT_FALSE(
      config.IsActuationAllowed(kWsOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kExampleOrigin));

  // Only navigation from ws:// -> wss:// allowed.
  EXPECT_TRUE(
      config.IsActuationAllowed(kWssOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsNavigationAllowed(kWsOrigin, kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWssOrigin, kWsOrigin));

  // Navigate to wss:// with different host should not be allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteWsOrigin,
                                         ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kWssOrigin, kCrossSiteWssOrigin));
}

TEST_F(TaskPolicyConfigTest, ToDebugStringEmpty) {
  TaskPolicyConfig config;

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(R"({"rules": {}})"));
}

TEST_F(TaskPolicyConfigTest, ToDebugStringWildcard) {
  TaskPolicyConfig config(
      {
          {WildcardLocation(), CreateRule({}, {Rule::Resource::kSession},
                                          {ClientTool(TestTool::kClick)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(ExpectedJsonWithTool(
                                         R"json({
        "rules": {
            "Wildcard": {
                "allowed_tools": [$1],
                "accessible_resources": ["RESOURCE_SESSION"],
                "navigation_sources": []
            }
        }
    })json",
                                         ClientTool(TestTool::kClick))));
}

TEST_F(TaskPolicyConfigTest, ToDebugStringSiteWithNavigationSource) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({OriginLocation(GURL("https://a.example.com"))},
                      {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(ExpectedJsonWithTool(
                                         R"json({
        "rules": {
            "Site(https://example.com)": {
                "allowed_tools": [$1],
                "accessible_resources": ["RESOURCE_SESSION"],
                "navigation_sources": ["Origin(https://a.example.com)"]
            }
        }
  })json",
                                         ClientTool(TestTool::kClick))));
}

TEST_F(TaskPolicyConfigTest, ToDebugStringMultipleRules) {
  TaskPolicyConfig config(
      {
          {SiteLocation(GURL("https://example.com")),
           CreateRule({OriginLocation(GURL("https://a.example.com"))},
                      {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick)})},
          {SiteLocation(GURL("https://foo.com")),
           CreateRule({OriginLocation(GURL("https://bar.example.com")),
                       SiteLocation(GURL("https://other.com"))},
                      {}, {ClientTool(TestTool::kClick)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_THAT(config.ToDebugValue(), base::test::IsJson(ExpectedJsonWithTool(
                                         R"json({
        "rules": {
            "Site(https://example.com)": {
                "allowed_tools": [$1],
                "accessible_resources": ["RESOURCE_SESSION"],
                "navigation_sources": ["Origin(https://a.example.com)"]
            },
            "Site(https://foo.com)": {
                "allowed_tools": [$1],
                "accessible_resources": [],
                "navigation_sources": ["Origin(https://bar.example.com)",
                                      "Site(https://other.com)"]
            }
        }
  })json",
                                         ClientTool(TestTool::kClick))));
}

TEST_F(TaskPolicyConfigTest, ClientToolDomainAndComparison) {
  ClientTool click1(TestTool::kClick);
  ClientTool click2(TestTool::kClick);
  ClientTool type(TestTool::kType);
  ClientTool other(OtherTestTool::kOther);

  EXPECT_EQ(click1, click2);
  EXPECT_NE(click1, type);
  EXPECT_NE(click1, other);

  EXPECT_TRUE(click1.IsSameDomain(type));
  EXPECT_FALSE(click1.IsSameDomain(other));

  EXPECT_EQ(click1.GetTool<TestTool>(), TestTool::kClick);
  EXPECT_EQ(type.GetTool<TestTool>(), TestTool::kType);
}

TEST_F(TaskPolicyConfigTest, AllowedTools_FineGrainedActuation) {
  TaskPolicyConfig config(
      {
          {Location(kExampleSite), CreateRule({}, {Rule::Resource::kSession},
                                              {ClientTool(TestTool::kClick),
                                               ClientTool(TestTool::kType)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kType)));
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin,
                                         ClientTool(TestTool::kNavigate)));

  // Unmatched origin is fail-closed.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin,
                                         ClientTool(TestTool::kClick)));

  // Navigation requires the navigate tool.
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, AllowedTools_EmptyIsFailClosed) {
  TaskPolicyConfig config(
      {
          {Location(kExampleSite),
           CreateRule({}, {Rule::Resource::kSession}, {})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, AllowedTools_RequiresSessionResource) {
  TaskPolicyConfig config(
      {
          {Location(kExampleSite),
           CreateRule({}, {}, {ClientTool(TestTool::kClick)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(TaskPolicyConfigTest, AllowedTools_OriginPrecedenceOverSite) {
  TaskPolicyConfig config(
      {
          {Location(kExampleOrigin),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick)})},
          {Location(kExampleSite), CreateRule({}, {Rule::Resource::kSession},
                                              {ClientTool(TestTool::kClick),
                                               ClientTool(TestTool::kType)})},
      },
      ClientTool(TestTool::kNavigate));

  // Exact origin match restricts to kClick only.
  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kType)));

  // Other subdomain falls back to site rule which allows both.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                        ClientTool(TestTool::kClick)));
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin,
                                        ClientTool(TestTool::kType)));
}

TEST_F(TaskPolicyConfigTest, AllowedTools_MultipleToolDomainsInRuleCrashes) {
  EXPECT_CHECK_DEATH(TaskPolicyConfig(
      {
          {Location(kExampleOrigin),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kType),
                       ClientTool(OtherTestTool::kOther)})},
      },
      ClientTool(TestTool::kNavigate)));
}

TEST_F(TaskPolicyConfigTest,
       AllowedTools_MultipleToolDomainsAcrossRulesCrashes) {
  EXPECT_CHECK_DEATH(TaskPolicyConfig(
      {
          {Location(kExampleOrigin), CreateRule({}, {Rule::Resource::kSession},
                                                {ClientTool(TestTool::kType)})},
          {Location(kExampleDifferentSubdomainOrigin),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(OtherTestTool::kOther)})},
      },
      ClientTool(TestTool::kNavigate)));
}

TEST_F(TaskPolicyConfigTest, AllowedTools_OtherDomainWithSameValueIsBlocked) {
  static_assert(static_cast<int>(TestTool::kClick) ==
                static_cast<int>(OtherTestTool::kOther));

  TaskPolicyConfig config(
      {
          {Location(kExampleOrigin),
           CreateRule({}, {Rule::Resource::kSession},
                      {ClientTool(TestTool::kClick)})},
      },
      ClientTool(TestTool::kNavigate));

  EXPECT_TRUE(
      config.IsActuationAllowed(kExampleOrigin, ClientTool(TestTool::kClick)));
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin,
                                         ClientTool(OtherTestTool::kOther)));
}

}  // namespace origin_gating
