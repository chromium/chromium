// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/utils.h"

#include <optional>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace enterprise_net {

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pair;

// Constants for test values.
constexpr char kTestPvdId[] = "api.example.com";

constexpr char kTestProxyHost1[] = "proxy1.example.com:443";
constexpr char kTestProxyHost2[] = "proxy2.example.com:443";
constexpr char kTestProxyHost3[] = "proxy3.example.com:443";

constexpr char kTestProxyIdentity1[] = "test-proxy-1";
constexpr char kTestProxyIdentity2[] = "test-proxy-2";
constexpr char kTestProxyIdentity3[] = "test-proxy-3";

constexpr char kTestDomain[] = "test.domain.com";
constexpr char kTestSubnet[] = "10.0.0.0/8";
constexpr uint16_t kTestPort = 443;
constexpr char kTestProfileId[] = "test-profile-id";
constexpr char kTestAcceptLanguages[] = "en-US,en;q=0.9";

constexpr char kTestHeaderKeyResourceKey[] = "x-resource-key";
constexpr char kTestHeaderKeyProfileId[] = "x-profile-id";
constexpr char kTestHeaderKeyLanguages[] = "x-languages";

net::ProxyChain MakeHttpsProxyChain(const std::string& host_and_port) {
  return net::ProxyChain(net::ProxyUriToProxyServer(
      host_and_port, net::ProxyServer::SCHEME_HTTPS));
}

net::ProxyChain MakeHttpProxyChain(const std::string& host_and_port) {
  return net::ProxyChain(
      net::ProxyUriToProxyServer(host_and_port, net::ProxyServer::SCHEME_HTTP));
}

net::ProxyChain MakeSocks5ProxyChain(const std::string& host_and_port) {
  return net::ProxyChain(net::ProxyUriToProxyServer(
      host_and_port, net::ProxyServer::SCHEME_SOCKS5));
}

ProvisioningDomainProxyConfig::RoutingRule MakeRoutingRule(
    std::vector<std::string> proxies,
    net::ProxyHostMatchingRules matchers) {
  return ProvisioningDomainProxyConfig::RoutingRule(std::move(proxies),
                                                    std::move(matchers));
}

// Constructs a comprehensive valid PvD JSON response string matching the
// official protocol draft.
std::string GetValidPvdJsonResponse() {
  return base::StringPrintf(
      R"({
  "identifier": "%s",
  "expires": "2026-03-01T12:00:00Z",
  "proxies": [
    {
      "protocol": "https-connect",
      "proxy": "%s",
      "identifier": "%s",
      "google_chrome": {
        "auth": {
          "type": "profile_bearer_token",
          "scope": "cloud_secure_gateway"
        },
        "extra_headers": [
          {
            "key": "x-chrome-custom",
            "constant": "custom-value"
          },
          {
            "key": "x-profile-id",
            "variable": "${profile_id}"
          }
        ]
      }
    },
    {
      "protocol": "https-connect",
      "proxy": "%s",
      "identifier": "%s"
    },
    {
      "protocol": "https-connect",
      "proxy": "%s",
      "identifier": "%s"
    }
  ],
  "proxy-match": [
    {
      "domains": ["%s"],
      "subnets": ["%s"],
      "ports": [%d],
      "proxies": ["%s"]
    },
    {
      "domains": ["multi.domain.com"],
      "proxies": ["%s", "%s", "%s"]
    },
    {
      "domains": ["bypass.domain.com"],
      "proxies": []
    },
    {
      "proxies": ["%s", "%s"]
    }
  ]
})",
      kTestPvdId, kTestProxyHost1, kTestProxyIdentity1, kTestProxyHost2,
      kTestProxyIdentity2, kTestProxyHost3, kTestProxyIdentity3, kTestDomain,
      kTestSubnet, kTestPort, kTestProxyIdentity1, kTestProxyIdentity1,
      kTestProxyIdentity2, kTestProxyIdentity3, kTestProxyIdentity2,
      kTestProxyIdentity3);
}

void ExpectHeader(const net::HttpRequestHeaders& headers,
                  const std::string& key,
                  const std::string& expected_value) {
  std::optional<std::string> val = headers.GetHeader(key);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, expected_value);
}

TEST(ParseProvisioningDomainProxyProtocolTest, MapsProtocolStringsCorrectly) {
  EXPECT_EQ(net::ProxyServer::SCHEME_HTTP,
            ParseProvisioningDomainProxyProtocol("http-connect"));
  EXPECT_EQ(net::ProxyServer::SCHEME_HTTPS,
            ParseProvisioningDomainProxyProtocol("https-connect"));
  EXPECT_EQ(net::ProxyServer::SCHEME_SOCKS5,
            ParseProvisioningDomainProxyProtocol("socks5"));
  EXPECT_EQ(net::ProxyServer::SCHEME_HTTP,
            ParseProvisioningDomainProxyProtocol("HTTP-CONNECT"));
  EXPECT_EQ(net::ProxyServer::SCHEME_HTTPS,
            ParseProvisioningDomainProxyProtocol("HTTPS-CONNECT"));

  // Unsupported or invalid protocols return SCHEME_INVALID.
  EXPECT_EQ(net::ProxyServer::SCHEME_INVALID,
            ParseProvisioningDomainProxyProtocol("connect-udp"));
  EXPECT_EQ(net::ProxyServer::SCHEME_INVALID,
            ParseProvisioningDomainProxyProtocol("connect-ip"));
  EXPECT_EQ(net::ProxyServer::SCHEME_INVALID,
            ParseProvisioningDomainProxyProtocol("connect-tcp"));
  EXPECT_EQ(net::ProxyServer::SCHEME_INVALID,
            ParseProvisioningDomainProxyProtocol("ftp"));
  EXPECT_EQ(net::ProxyServer::SCHEME_INVALID,
            ParseProvisioningDomainProxyProtocol("invalid"));
}

TEST(ResolveExtraHeadersTest, ExpandsPlaceholdersCorrectly) {
  ProvisioningDomainConfig policy;
  policy.pvd_id = "example.com";
  policy.extra_headers = {
      ProxyExtraHeader(kTestHeaderKeyResourceKey, "projects/default"),
      ProxyExtraHeader(kTestHeaderKeyProfileId, "${profile_id}"),
      ProxyExtraHeader(kTestHeaderKeyLanguages, "${accept_language}"),
  };

  net::HttpRequestHeaders headers =
      ResolveExtraHeaders(policy, kTestProfileId, kTestAcceptLanguages);

  ExpectHeader(headers, kTestHeaderKeyResourceKey, "projects/default");
  ExpectHeader(headers, kTestHeaderKeyProfileId, kTestProfileId);
  ExpectHeader(headers, kTestHeaderKeyLanguages, kTestAcceptLanguages);
}

TEST(ParseProxyProvisioningDomainPolicyTest, ParsesValidPolicyDict) {
  std::string policy_json = R"({
    "pvd_id": "example.pvd.com",
    "auth_config": {
      "type": "PROFILE_BEARER_TOKEN",
      "scope": "CLOUD_SECURE_GATEWAY"
    },
    "extra_headers": [
      {
        "key": "x-custom-key",
        "value": "custom-value"
      }
    ]
  })";

  std::optional<base::DictValue> domain_dict =
      base::JSONReader::ReadDict(policy_json, 0);
  ASSERT_TRUE(domain_dict.has_value());

  std::optional<ProvisioningDomainConfig> policy =
      ParseProxyProvisioningDomainPolicy(*domain_dict);

  ASSERT_TRUE(policy.has_value());

  ProvisioningDomainConfig expected;
  expected.pvd_id = "example.pvd.com";
  expected.auth_config = ProxyAuthConfig{AuthType::kProfileBearerToken,
                                         AuthScope::kCloudSecureGateway};
  expected.extra_headers = {ProxyExtraHeader("x-custom-key", "custom-value")};

  EXPECT_EQ(*policy, expected);
}

TEST(ParseProxyProvisioningDomainPolicyTest, RejectsInvalidPolicyDict) {
  base::DictValue missing_pvd_id;
  missing_pvd_id.Set("some_other_key", "value");

  std::optional<ProvisioningDomainConfig> policy =
      ParseProxyProvisioningDomainPolicy(missing_pvd_id);
  EXPECT_FALSE(policy.has_value());
}

TEST(ParseProvisioningDomainConfigTest, ParsesValidPvdResponse) {
  std::optional<ProvisioningDomainProxyConfig> config =
      ParseProvisioningDomainConfig(GetValidPvdJsonResponse());

  ASSERT_TRUE(config.has_value());

  ProvisioningDomainProxyConfig expected;
  expected.pvd_id = kTestPvdId;
  ASSERT_TRUE(
      base::Time::FromString("2026-03-01T12:00:00Z", &expected.expires));
  expected.state = ProvisioningDomainProxyConfig::State::kRefreshNeeded;

  using ProxyEndpoint = ProvisioningDomainProxyConfig::ProxyEndpoint;

  expected.proxy_endpoints.insert(
      {kTestProxyIdentity1,
       ProxyEndpoint(
           MakeHttpsProxyChain(kTestProxyHost1),
           ProxyAuthConfig{AuthType::kProfileBearerToken,
                           AuthScope::kCloudSecureGateway},
           {
               ProxyExtraHeader("x-chrome-custom", "custom-value",
                                ProxyExtraHeader::HeaderType::kConstant),
               ProxyExtraHeader("x-profile-id", "${profile_id}",
                                ProxyExtraHeader::HeaderType::kVariable),
           })});

  expected.proxy_endpoints.insert(
      {kTestProxyIdentity2,
       ProxyEndpoint(MakeHttpsProxyChain(kTestProxyHost2))});
  expected.proxy_endpoints.insert(
      {kTestProxyIdentity3,
       ProxyEndpoint(MakeHttpsProxyChain(kTestProxyHost3))});

  net::ProxyHostMatchingRules matchers1;
  matchers1.AddRuleFromString(
      base::StrCat({kTestDomain, ":", base::NumberToString(kTestPort)}));
  matchers1.AddRuleFromString(
      base::StrCat({kTestSubnet, ":", base::NumberToString(kTestPort)}));
  expected.routing_rules.push_back(
      MakeRoutingRule({kTestProxyIdentity1}, std::move(matchers1)));

  net::ProxyHostMatchingRules matchers2;
  matchers2.AddRuleFromString("multi.domain.com");
  expected.routing_rules.push_back(MakeRoutingRule(
      {kTestProxyIdentity1, kTestProxyIdentity2, kTestProxyIdentity3},
      std::move(matchers2)));

  net::ProxyHostMatchingRules matchers3;
  matchers3.AddRuleFromString("bypass.domain.com");
  expected.routing_rules.push_back(
      MakeRoutingRule({"DIRECT"}, std::move(matchers3)));

  net::ProxyHostMatchingRules matchers4;
  matchers4.AddRuleFromString("*");
  expected.routing_rules.push_back(MakeRoutingRule(
      {kTestProxyIdentity2, kTestProxyIdentity3}, std::move(matchers4)));

  EXPECT_EQ(*config, expected);

  // Verify that ToDynamicRoutingConfig exports clean network stack rules.
  net::ProxyConfig::DynamicRoutingConfig dynamic_config =
      config->ToDynamicRoutingConfig();
  ASSERT_EQ(4u, dynamic_config.routing_rules.size());
  EXPECT_EQ(expected.routing_rules[0].destination_matchers,
            dynamic_config.routing_rules[0].destination_matchers);
  EXPECT_EQ(MakeHttpsProxyChain(kTestProxyHost1),
            dynamic_config.routing_rules[0].proxy_list.First());
}

// Test that the parsing function handles various protocols correctly.
// - http-connect, socks5, etc. should be parsed correctly.
// - Unknown/unsupported protocols should be ignored.
TEST(ParseProvisioningDomainConfigTest, HandlesVariousProtocols) {
  std::string http_json = R"({
    "identifier": "example.com",
    "proxies": [
      {
        "protocol": "http-connect",
        "proxy": "proxy1.example.com:80",
        "identifier": "http-proxy"
      },
      {
        "protocol": "socks5",
        "proxy": "proxy2.example.com:1080",
        "identifier": "socks-proxy"
      }
    ],
    "proxy-match": []
  })";

  std::optional<ProvisioningDomainProxyConfig> config =
      ParseProvisioningDomainConfig(http_json);
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(2u, config->proxy_endpoints.size());

  EXPECT_EQ(MakeHttpProxyChain("proxy1.example.com:80"),
            config->proxy_endpoints["http-proxy"].proxy_chain);
  EXPECT_EQ(MakeSocks5ProxyChain("proxy2.example.com:1080"),
            config->proxy_endpoints["socks-proxy"].proxy_chain);

  std::string invalid_proto_json = R"({
    "identifier": "example.com",
    "proxies": [
      {
        "protocol": "ftp",
        "proxy": "proxy.example.com:21",
        "identifier": "ftp-proxy"
      }
    ],
    "proxy-match": []
  })";
  std::optional<ProvisioningDomainProxyConfig> invalid_config =
      ParseProvisioningDomainConfig(invalid_proto_json);
  ASSERT_TRUE(invalid_config.has_value());
  EXPECT_TRUE(invalid_config->proxy_endpoints.empty());

  std::string missing_proto_json = R"({
    "identifier": "example.com",
    "proxies": [
      {
        "proxy": "proxy.example.com:443",
        "identifier": "missing-proto-proxy"
      }
    ],
    "proxy-match": []
  })";
  std::optional<ProvisioningDomainProxyConfig> missing_proto_config =
      ParseProvisioningDomainConfig(missing_proto_json);
  ASSERT_TRUE(missing_proto_config.has_value());
  EXPECT_TRUE(missing_proto_config->proxy_endpoints.empty());
}

TEST(ParseProvisioningDomainConfigTest, HandlesMissingOptionalFields) {
  std::string minimal_json = R"({
    "identifier": "example.com",
    "proxies": [
      {
        "protocol": "https-connect",
        "proxy": "proxy1.example.com:443"
      }
    ],
    "proxy-match": []
  })";

  std::optional<ProvisioningDomainProxyConfig> config =
      ParseProvisioningDomainConfig(minimal_json);
  ASSERT_TRUE(config.has_value());
  ASSERT_EQ(1u, config->proxy_endpoints.size());

  // Identifier omitted in "proxies", so it falls back to the proxy URI string.
  const auto& endpoint = config->proxy_endpoints["proxy1.example.com:443"];
  EXPECT_EQ(MakeHttpsProxyChain("proxy1.example.com:443"),
            endpoint.proxy_chain);
  EXPECT_FALSE(endpoint.auth.has_value());
  EXPECT_TRUE(endpoint.extra_headers.empty());
}

TEST(ParseProvisioningDomainConfigTest, HandlesMalformedJson) {
  std::optional<ProvisioningDomainProxyConfig> config =
      ParseProvisioningDomainConfig("{invalid json");
  EXPECT_FALSE(config.has_value());

  // Missing required "proxies" key
  std::optional<ProvisioningDomainProxyConfig> missing_proxies =
      ParseProvisioningDomainConfig(R"({"proxy-match": []})");
  EXPECT_FALSE(missing_proxies.has_value());
}

TEST(FindMatchingProxyEndpointTest, FindsFirstMatchingEndpoint) {
  std::optional<ProvisioningDomainProxyConfig> config =
      ParseProvisioningDomainConfig(GetValidPvdJsonResponse());
  ASSERT_TRUE(config.has_value());

  // Request to test.domain.com:443 via proxy1 should match the first routing
  // rule.
  const auto* endpoint1 =
      FindMatchingProxyEndpoint(*config, GURL("https://test.domain.com/path"),
                                MakeHttpsProxyChain(kTestProxyHost1));
  ASSERT_NE(nullptr, endpoint1);
  EXPECT_EQ(MakeHttpsProxyChain(kTestProxyHost1), endpoint1->proxy_chain);
  ASSERT_TRUE(endpoint1->auth.has_value());
  EXPECT_EQ(AuthType::kProfileBearerToken, endpoint1->auth->type);

  // Request to any other domain should fall through to the wildcard ("*") rule
  // matching proxy2 or proxy3.
  const auto* endpoint2 =
      FindMatchingProxyEndpoint(*config, GURL("https://other.domain.com/path"),
                                MakeHttpsProxyChain(kTestProxyHost2));
  ASSERT_NE(nullptr, endpoint2);
  EXPECT_EQ(MakeHttpsProxyChain(kTestProxyHost2), endpoint2->proxy_chain);

  // Request for a proxy chain that is not in any matching rule should return
  // nullptr.
  const auto* non_matching =
      FindMatchingProxyEndpoint(*config, GURL("https://test.domain.com/path"),
                                MakeHttpsProxyChain("unknown.proxy.com:443"));
  EXPECT_EQ(nullptr, non_matching);
}

}  // namespace
}  // namespace enterprise_net
