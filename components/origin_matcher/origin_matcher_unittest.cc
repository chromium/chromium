// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_matcher/origin_matcher.h"

#include "base/test/scoped_feature_list.h"
#include "components/origin_matcher/origin_matcher_internal.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

// List below origin_matcher_internal.h, so OriginMatcherRule's a complete type.
#include "components/origin_matcher/origin_matcher.mojom.h"

namespace origin_matcher {

SubdomainMatchingRule::SubdomainMatchingRule(const std::string& scheme,
                                             const std::string& optional_host,
                                             int optional_port,
                                             bool for_test)
    : OriginMatcherRule(OriginMatcherRuleType::kSubdomain),
      scheme_(scheme),
      optional_host_(optional_host),
      optional_port_(optional_port) {}

class OriginMatcherTest : public testing::Test {
 public:
  void SetUp() override {
    scheme_registry_ = std::make_unique<url::ScopedSchemeRegistryForTests>();
    url::EnableNonStandardSchemesForAndroidWebView();
  }

  static url::Origin CreateOriginFromString(const std::string& url) {
    return url::Origin::Create(GURL(url));
  }

 private:
  std::unique_ptr<url::ScopedSchemeRegistryForTests> scheme_registry_;
};

TEST_F(OriginMatcherTest, InvalidInputs) {
  OriginMatcher matcher;
  // Empty string is invalid.
  EXPECT_FALSE(matcher.AddRuleFromString(""));
  // Scheme doesn't present.
  EXPECT_FALSE(matcher.AddRuleFromString("example.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("://example.com"));
  // Scheme doesn't do wildcard matching.
  EXPECT_FALSE(matcher.AddRuleFromString("*://example.com"));
  // URL like rule is invalid.
  EXPECT_FALSE(matcher.AddRuleFromString("https://www.example.com/index.html"));
  EXPECT_FALSE(matcher.AddRuleFromString("http://192.168.0.1/*"));
  // Only accept hostname pattern starts with "*." if there is a "*" inside.
  EXPECT_FALSE(matcher.AddRuleFromString("https://*foobar.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://x.*.y.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://*example.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://e*xample.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://*"));
  EXPECT_FALSE(matcher.AddRuleFromString("http://*"));
  // Invalid port.
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:**"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:-1"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:+443"));
  // Empty hostname pattern for http/https.
  EXPECT_FALSE(matcher.AddRuleFromString("http://"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://:80"));
  // No IP block support.
  EXPECT_FALSE(matcher.AddRuleFromString("https://192.168.0.0/16"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://fefe:13::abc/33"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://:1"));
  // Invalid IP address.
  EXPECT_FALSE(matcher.AddRuleFromString("http://[a:b:*]"));
  EXPECT_FALSE(matcher.AddRuleFromString("http://[a:b:*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://fefe:13::*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://fefe:13:*/33"));
  // Custom scheme with host and/or port are invalid. This is because in
  // WebView, all the URI with the same custom scheme belong to one origin.
  EXPECT_FALSE(matcher.AddRuleFromString("x-mail://hostname:80"));
  EXPECT_FALSE(matcher.AddRuleFromString("x-mail://hostname"));
  EXPECT_FALSE(matcher.AddRuleFromString("x-mail://*"));
  // file scheme with "host"
  EXPECT_FALSE(matcher.AddRuleFromString("file://host"));
  EXPECT_FALSE(matcher.AddRuleFromString("file://*"));
}

TEST_F(OriginMatcherTest, ExactMatching) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://www.example.com:99"));
  EXPECT_EQ("https://www.example.com:99", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(OriginMatcherTest, SchemeDefaultPortHttp) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("http://www.example.com"));
  EXPECT_EQ("http://www.example.com:80", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:80")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://music.example.com:80")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://music.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:80")));
}

TEST_F(OriginMatcherTest, SchemeDefaultPortHttps) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://www.example.com"));
  EXPECT_EQ("https://www.example.com:443", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:443")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:443")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(OriginMatcherTest, SubdomainMatching) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://*.example.com"));
  EXPECT_EQ("https://*.example.com:443", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:443")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://music.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:443")));
  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("https://music.video.radio.example.com")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("ftp://www.example.com")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("https://example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(OriginMatcherTest, SubdomainMatching2) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("http://*.www.example.com"));
  EXPECT_EQ("http://*.www.example.com:80", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://abc.www.example.com:80")));
  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("http://music.video.www.example.com")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("ftp://www.example.com")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("https://example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(OriginMatcherTest, PunyCode) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("http://*.xn--fsqu00a.com"));

  // Chinese domain example.com
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("http://www.例子.com")));
}

TEST_F(OriginMatcherTest, IPv4AddressMatching) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://192.168.0.1"));
  EXPECT_EQ("https://192.168.0.1:443", matcher.rules()[0]->ToString());

  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("https://192.168.0.1")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://192.168.0.1:443")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://192.168.0.1:99")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("http://192.168.0.1")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("http://192.168.0.2")));
}

TEST_F(OriginMatcherTest, IPv6AddressMatching) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://[3ffe:2a00:100:7031:0:0::1]"));
  // Note that the IPv6 address is canonicalized.
  EXPECT_EQ("https://[3ffe:2a00:100:7031::1]:443",
            matcher.rules()[0]->ToString());

  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("https://[3ffe:2a00:100:7031::1]")));
  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("https://[3ffe:2a00:100:7031::1]:443")));

  EXPECT_FALSE(matcher.Matches(
      CreateOriginFromString("http://[3ffe:2a00:100:7031::1]")));
  EXPECT_FALSE(matcher.Matches(
      CreateOriginFromString("http://[3ffe:2a00:100:7031::1]:443")));
  EXPECT_FALSE(matcher.Matches(
      CreateOriginFromString("https://[3ffe:2a00:100:7031::1]:8080")));
}

TEST_F(OriginMatcherTest, WildcardMatchesEveryOrigin) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("*"));

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://foo.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:8080")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("http://192.168.0.1")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://192.168.0.1:8080")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("https://[a:b:c:d::]")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://[a:b:c:d::]:8080")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("ftp://example.com")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("about:blank")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString(
      "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ%3D%3D")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("file:///usr/local/a.txt")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString(
      "blob:http://127.0.0.1:8080/0530b9d1-c1c2-40ff-9f9c-c57336646baa")));
}

TEST_F(OriginMatcherTest, FileOrigin) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("file://"));

  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("file:///sdcard")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("file:///android_assets")));
}

TEST_F(OriginMatcherTest, CustomSchemeOrigin) {
  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("x-mail://"));

  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("x-mail://hostname")));
}

namespace {

void CompareMatcherRules(const OriginMatcherRule& r1,
                         const OriginMatcherRule& r2) {
  ASSERT_EQ(r1.type(), r2.type());
  if (r1.type() == origin_matcher::OriginMatcherRuleType::kAny) {
    return;
  }
  const SubdomainMatchingRule& s1 =
      static_cast<const SubdomainMatchingRule&>(r1);
  const SubdomainMatchingRule& s2 =
      static_cast<const SubdomainMatchingRule&>(r2);
  EXPECT_EQ(s1.scheme(), s2.scheme());
  EXPECT_EQ(s1.optional_host(), s2.optional_host());
  EXPECT_EQ(s1.optional_port(), s2.optional_port());
}

void CompareMatchers(const OriginMatcher& m1, const OriginMatcher& m2) {
  ASSERT_EQ(m1.rules().size(), m2.rules().size());
  for (size_t i = 0; i < m1.rules().size(); ++i) {
    ASSERT_NO_FATAL_FAILURE(
        CompareMatcherRules(*(m1.rules()[i].get()), *(m2.rules())[i].get()));
  }
}

}  // namespace

TEST_F(OriginMatcherTest, SerializeAndDeserializeMatchAll) {
  OriginMatcher matcher;
  OriginMatcher deserialized;
  ASSERT_TRUE(matcher.AddRuleFromString("*"));
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::OriginMatcher>(
      matcher, deserialized));
  ASSERT_NO_FATAL_FAILURE(CompareMatchers(matcher, deserialized));
}

TEST_F(OriginMatcherTest, SerializeAndDeserializeSubdomainMatcher) {
  OriginMatcher matcher;
  OriginMatcher deserialized;
  ASSERT_TRUE(matcher.AddRuleFromString("https://*.example.com"));
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::OriginMatcher>(
      matcher, deserialized));
  ASSERT_NO_FATAL_FAILURE(CompareMatchers(matcher, deserialized));
}

TEST_F(OriginMatcherTest, SerializeAndDeserializeInvalidSubdomain) {
  OriginMatcher matcher;
  OriginMatcher deserialized;
  {
    OriginMatcher::RuleList rules;
    // The subdomain is not allowed to have a '/'.
    rules.push_back(std::make_unique<SubdomainMatchingRule>(
        "http", "bogus/host", 100, true));
    matcher.SetRules(std::move(rules));
  }
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::OriginMatcher>(
      matcher, deserialized));
}

TEST_F(OriginMatcherTest, SerializeAndDeserializeInvalidScheme) {
  OriginMatcher matcher;
  OriginMatcher deserialized;
  {
    OriginMatcher::RuleList rules;
    // The scheme can not be empty.
    rules.push_back(std::make_unique<SubdomainMatchingRule>(std::string(),
                                                            "host", 101, true));
    matcher.SetRules(std::move(rules));
  }
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::OriginMatcher>(
      matcher, deserialized));
}

TEST_F(OriginMatcherTest, SerializeAndDeserializeTooManyWildcards) {
  OriginMatcher matcher;
  OriginMatcher deserialized;
  {
    OriginMatcher::RuleList rules;
    // Only one wildcard is allowed.
    rules.push_back(
        std::make_unique<SubdomainMatchingRule>("http", "**", 101, true));
    matcher.SetRules(std::move(rules));
  }
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::OriginMatcher>(
      matcher, deserialized));
}

TEST_F(OriginMatcherTest, SerializeAndDeserializeInvalidWildcard) {
  OriginMatcher matcher;
  OriginMatcher deserialized;
  {
    OriginMatcher::RuleList rules;
    // The wild card must be at the front.
    rules.push_back(
        std::make_unique<SubdomainMatchingRule>("http", "ab*", 101, true));
    matcher.SetRules(std::move(rules));
  }
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::OriginMatcher>(
      matcher, deserialized));
}

TEST_F(OriginMatcherTest, SerializeAndDeserializeValidWildcard) {
  OriginMatcher matcher;
  OriginMatcher deserialized;
  {
    OriginMatcher::RuleList rules;
    // The wild card must be at the front.
    rules.push_back(
        std::make_unique<SubdomainMatchingRule>("http", "*.ab", 101, true));
    matcher.SetRules(std::move(rules));
  }
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::OriginMatcher>(
      matcher, deserialized));
  ASSERT_NO_FATAL_FAILURE(CompareMatchers(matcher, deserialized));
}

TEST_F(OriginMatcherTest, CopyAndAssign) {

  OriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("*"));
  EXPECT_TRUE(matcher.AddRuleFromString("https://*.example.com"));

  OriginMatcher assigned = matcher;
  EXPECT_EQ(2u, assigned.rules().size());
  EXPECT_EQ("*", assigned.rules()[0]->ToString());
  EXPECT_EQ("https://*.example.com:443", assigned.rules()[1]->ToString());
  EXPECT_EQ(matcher, assigned);
}

TEST_F(OriginMatcherTest, Equality) {
  OriginMatcher empty1;
  OriginMatcher empty2;
  EXPECT_EQ(empty1, empty2);

  OriginMatcher wildcard1;
  OriginMatcher wildcard2;
  ASSERT_TRUE(wildcard1.AddRuleFromString("*"));
  ASSERT_TRUE(wildcard2.AddRuleFromString("*"));
  EXPECT_EQ(wildcard1, wildcard2);
  EXPECT_NE(empty1, wildcard1);

  OriginMatcher example1;
  OriginMatcher example2;
  ASSERT_TRUE(example1.AddRuleFromString("https://EXAMPLE.COM"));
  ASSERT_TRUE(example2.AddRuleFromString("https://example.com:443"));
  EXPECT_EQ(example1, example2);
  EXPECT_NE(wildcard1, example1);

  OriginMatcher different_scheme;
  ASSERT_TRUE(different_scheme.AddRuleFromString("http://example.com:443"));
  EXPECT_NE(example1, different_scheme);

  OriginMatcher different_host;
  ASSERT_TRUE(different_host.AddRuleFromString("https://*.example.com:443"));
  EXPECT_NE(example1, different_host);

  OriginMatcher different_port;
  ASSERT_TRUE(different_port.AddRuleFromString("https://example.com:8443"));
  EXPECT_NE(example1, different_port);
}

TEST_F(OriginMatcherTest, EqualityOrderIndependent) {
  OriginMatcher m1;
  ASSERT_TRUE(m1.AddRuleFromString("https://a.example.com"));
  ASSERT_TRUE(m1.AddRuleFromString("http://*.b.example.com:8080"));
  ASSERT_TRUE(m1.AddRuleFromString("file://"));

  OriginMatcher m2;
  ASSERT_TRUE(m2.AddRuleFromString("file://"));
  ASSERT_TRUE(m2.AddRuleFromString("http://*.b.example.com:8080"));
  ASSERT_TRUE(m2.AddRuleFromString("https://a.example.com:443"));

  EXPECT_EQ(m1, m2);

  OriginMatcher with_wildcard1;
  ASSERT_TRUE(with_wildcard1.AddRuleFromString("https://a.example.com"));
  ASSERT_TRUE(with_wildcard1.AddRuleFromString("http://*.b.example.com:8080"));
  ASSERT_TRUE(with_wildcard1.AddRuleFromString("file://"));
  ASSERT_TRUE(with_wildcard1.AddRuleFromString("*"));

  OriginMatcher with_wildcard2;
  ASSERT_TRUE(with_wildcard2.AddRuleFromString("*"));
  ASSERT_TRUE(with_wildcard2.AddRuleFromString("file://"));
  ASSERT_TRUE(with_wildcard2.AddRuleFromString("http://*.b.example.com:8080"));
  ASSERT_TRUE(with_wildcard2.AddRuleFromString("https://a.example.com:443"));

  EXPECT_EQ(with_wildcard1, with_wildcard2);
  EXPECT_NE(m1, with_wildcard1);
}

TEST_F(OriginMatcherTest, EqualityDuplicateIndependent) {
  OriginMatcher single;
  ASSERT_TRUE(single.AddRuleFromString("https://a.example.com"));
  ASSERT_TRUE(single.AddRuleFromString("https://b.example.com"));

  OriginMatcher duplicate_a;
  ASSERT_TRUE(duplicate_a.AddRuleFromString("https://a.example.com"));
  ASSERT_TRUE(duplicate_a.AddRuleFromString("https://A.EXAMPLE.COM:443"));
  ASSERT_TRUE(duplicate_a.AddRuleFromString("https://b.example.com"));
  EXPECT_EQ(2u, duplicate_a.rules().size());
  EXPECT_EQ(single, duplicate_a);

  OriginMatcher duplicate_b;
  ASSERT_TRUE(duplicate_b.AddRuleFromString("https://b.example.com"));
  ASSERT_TRUE(duplicate_b.AddRuleFromString("https://a.example.com"));
  ASSERT_TRUE(duplicate_b.AddRuleFromString("https://b.example.com:443"));
  EXPECT_EQ(2u, duplicate_b.rules().size());
  EXPECT_EQ(duplicate_a, duplicate_b);
}

TEST_F(OriginMatcherTest, MatchAllRuleSortedFirst) {
  OriginMatcher matcher;
  ASSERT_TRUE(matcher.AddRuleFromString("https://z.example.com"));
  ASSERT_TRUE(matcher.AddRuleFromString("file://"));
  ASSERT_TRUE(matcher.AddRuleFromString("http://a.example.com"));
  ASSERT_TRUE(matcher.AddRuleFromString("*"));

  ASSERT_EQ(4u, matcher.rules().size());
  EXPECT_EQ("*", matcher.rules()[0]->ToString());
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://other.example.com")));

  OriginMatcher from_rule_list;
  OriginMatcher::RuleList rules;
  rules.push_back(
      std::make_unique<SubdomainMatchingRule>("https", "example.com", 443));
  rules.push_back(std::make_unique<MatchAllOriginsRule>());
  from_rule_list.SetRules(std::move(rules));

  ASSERT_EQ(2u, from_rule_list.rules().size());
  EXPECT_EQ("*", from_rule_list.rules()[0]->ToString());
}

}  // namespace origin_matcher
