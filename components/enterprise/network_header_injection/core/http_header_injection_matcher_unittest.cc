// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_matcher.h"

#include <memory>

#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_custom_headers {

class HttpHeaderInjectionMatcherTest : public testing::Test {
 protected:
  HttpHeaderInjectionMatcherTest()
      : matcher_(HttpHeaderInjectionMatcher::Create()) {}

  std::unique_ptr<HttpHeaderInjectionMatcher> matcher_;
};

// Tests that an empty matcher returns no headers.
TEST_F(HttpHeaderInjectionMatcherTest, EmptyMatcher_ReturnsNoHeaders) {
  EXPECT_TRUE(matcher_->IsEmpty());
  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_TRUE(headers.IsEmpty());
}

// Tests that a leading dot in the domain pattern results in an exact match.
TEST_F(HttpHeaderInjectionMatcherTest, ExactMatch_MatchesOnlyExactHost) {
  // In URLBlocklist style, a leading dot means exact match (no subdomains).
  matcher_->UpdateRules({{.url_patterns = {".example.com"},
                          .headers = {{"X-Enterprise-Test", "SecretValue"}}}});

  EXPECT_FALSE(matcher_->IsEmpty());
  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("SecretValue", headers.GetHeader("X-Enterprise-Test"));

  headers = matcher_->GetHeadersForUrl(GURL("https://sub.example.com"));
  EXPECT_TRUE(headers.IsEmpty());
}

// Tests that a domain pattern without a leading dot matches subdomains.
TEST_F(HttpHeaderInjectionMatcherTest, WildcardMatch_MatchesSubdomains) {
  // In URLBlocklist style, no leading dot means matching subdomains by default.
  matcher_->UpdateRules({{.url_patterns = {"example.com"},
                          .headers = {{"X-Enterprise-Test", "SecretValue"}}}});

  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("SecretValue", headers.GetHeader("X-Enterprise-Test"));

  headers = matcher_->GetHeadersForUrl(GURL("https://sub.example.com"));
  EXPECT_EQ("SecretValue", headers.GetHeader("X-Enterprise-Test"));
}

// Tests that an exact match takes precedence over a subdomain match.
TEST_F(HttpHeaderInjectionMatcherTest, Precedence_ExactMatchBeatsWildcard) {
  matcher_->UpdateRules(
      {{.url_patterns = {"example.com"},
        .headers = {{"X-Enterprise-Test", "GeneralValue"}}},
       {.url_patterns = {".sub.example.com"},
        .headers = {{"X-Enterprise-Test", "SpecificValue"}}}});

  // For example.com, only Rule 1 matches.
  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("GeneralValue", headers.GetHeader("X-Enterprise-Test"));

  // For sub.example.com, both match, but Rule 2 is more specific (exact match).
  headers = matcher_->GetHeadersForUrl(GURL("https://sub.example.com"));
  EXPECT_EQ("SpecificValue", headers.GetHeader("X-Enterprise-Test"));
}

// Tests that a rule with a longer path takes precedence.
TEST_F(HttpHeaderInjectionMatcherTest, Precedence_LongestPathWins) {
  matcher_->UpdateRules({{.url_patterns = {"example.com"},
                          .headers = {{"X-Precedence-Test", "ShortPath"}}},
                         {.url_patterns = {"example.com/path"},
                          .headers = {{"X-Precedence-Test", "LongPath"}}}});

  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com/path"));
  EXPECT_EQ("LongPath", headers.GetHeader("X-Precedence-Test"));
}

// Tests that a specific domain takes precedence over a global wildcard.
TEST_F(HttpHeaderInjectionMatcherTest,
       Precedence_SpecificDomainBeatsGlobalWildcard) {
  matcher_->UpdateRules(
      {{.url_patterns = {"*"}, .headers = {{"X-Precedence-Test", "Wildcard"}}},
       {.url_patterns = {"example.com"},
        .headers = {{"X-Precedence-Test", "Specific"}}}});

  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("Specific", headers.GetHeader("X-Precedence-Test"));
}

// Tests that if two rules are equally specific, the later one wins.
TEST_F(HttpHeaderInjectionMatcherTest, Precedence_LaterRuleWinsOnTie) {
  matcher_->UpdateRules({{.url_patterns = {"example.com"},
                          .headers = {{"X-Precedence-Test", "FirstRule"}}},
                         {.url_patterns = {"example.com"},
                          .headers = {{"X-Precedence-Test", "SecondRule"}}}});

  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("SecondRule", headers.GetHeader("X-Precedence-Test"));
}

// Tests that headers from multiple matching rules accumulate.
TEST_F(HttpHeaderInjectionMatcherTest, Accumulation_MergesDifferentHeaders) {
  // Rule 1: Wildcard for example.com setting Header 1
  // Rule 2: Exact match for sub.example.com setting Header 2
  matcher_->UpdateRules(
      {{.url_patterns = {"example.com"}, .headers = {{"X-Test-1", "Value1"}}},
       {.url_patterns = {".sub.example.com"},
        .headers = {{"X-Test-2", "Value2"}}}});

  // For sub.example.com, both match, and headers should accumulate!
  auto headers = matcher_->GetHeadersForUrl(GURL("https://sub.example.com"));
  EXPECT_EQ("Value1", headers.GetHeader("X-Test-1"));
  EXPECT_EQ("Value2", headers.GetHeader("X-Test-2"));
}

// Tests that a single rule can have multiple patterns.
TEST_F(HttpHeaderInjectionMatcherTest, MultiplePatterns_MatchesAnyPattern) {
  matcher_->UpdateRules({{.url_patterns = {"example.com", "google.com"},
                          .headers = {{"X-Enterprise-Test", "SecretValue"}}}});

  EXPECT_FALSE(matcher_->IsEmpty());

  // Test first pattern
  auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("SecretValue", headers.GetHeader("X-Enterprise-Test"));

  // Test second pattern
  headers = matcher_->GetHeadersForUrl(GURL("https://google.com"));
  EXPECT_EQ("SecretValue", headers.GetHeader("X-Enterprise-Test"));

  // Test unrelated domain
  headers = matcher_->GetHeadersForUrl(GURL("https://yahoo.com"));
  EXPECT_TRUE(headers.IsEmpty());
}

// Tests that multiple rules, each containing multiple patterns, accumulate and
// resolve precedence correctly.
TEST_F(HttpHeaderInjectionMatcherTest,
       MultipleRulesAndPatterns_AccumulatesAndResolvesPrecedence) {
  matcher_->UpdateRules(
      {{.url_patterns = {"example.com", "google.com"},
        .headers = {{"X-Common-Header", "Value1"}, {"X-Unique-1", "A"}}},
       {.url_patterns = {".sub.example.com", "youtube.com"},
        .headers = {{"X-Common-Header", "Value2"}, {"X-Unique-2", "B"}}}});

  // 1. Test a domain matching only the first rule (e.g., example.com)
  {
    auto headers = matcher_->GetHeadersForUrl(GURL("https://example.com"));
    EXPECT_EQ("Value1", headers.GetHeader("X-Common-Header"));
    EXPECT_EQ("A", headers.GetHeader("X-Unique-1"));
    EXPECT_FALSE(headers.HasHeader("X-Unique-2"));
  }

  // 2. Test a domain matching both rules, where rule 2 takes precedence for
  // X-Common-Header (e.g., sub.example.com)
  {
    auto headers = matcher_->GetHeadersForUrl(GURL("https://sub.example.com"));
    EXPECT_EQ("Value2",
              headers.GetHeader("X-Common-Header"));  // exact match wins
    EXPECT_EQ("A", headers.GetHeader("X-Unique-1"));
    EXPECT_EQ("B", headers.GetHeader("X-Unique-2"));
  }

  // 3. Test a domain matching only the second rule (e.g., youtube.com)
  {
    auto headers = matcher_->GetHeadersForUrl(GURL("https://youtube.com"));
    EXPECT_EQ("Value2", headers.GetHeader("X-Common-Header"));
    EXPECT_EQ("B", headers.GetHeader("X-Unique-2"));
    EXPECT_FALSE(headers.HasHeader("X-Unique-1"));
  }
}

// Tests that HTTP header names are handled case-insensitively when resolving
// precedence and deduplicating injected headers.
TEST_F(HttpHeaderInjectionMatcherTest,
       CaseInsensitivity_DeduplicatesAndResolvesPrecedence) {
  matcher_->UpdateRules({{.url_patterns = {"example.com"},
                          .headers = {{"X-Common-Header", "Value1"}}},
                         {.url_patterns = {".sub.example.com"},
                          .headers = {{"x-common-header", "Value2"}}}});

  // Both rules match sub.example.com. The second rule (exact match) has higher
  // precedence and its header ("x-common-header") should overwrite the first
  // one
  // ("X-Common-Header").
  auto headers = matcher_->GetHeadersForUrl(GURL("https://sub.example.com"));

  // Verify case-insensitive retrieval works
  EXPECT_EQ("Value2", headers.GetHeader("X-Common-Header"));
  EXPECT_EQ("Value2", headers.GetHeader("x-common-header"));

  // Verify they were deduplicated and only one header is set
  net::HttpRequestHeaders::Iterator it(headers);
  int header_count = 0;
  while (it.GetNext()) {
    header_count++;
    EXPECT_TRUE(it.name() == "X-Common-Header" ||
                it.name() == "x-common-header");
    EXPECT_EQ("Value2", it.value());
  }
  EXPECT_EQ(1, header_count);
}

}  // namespace enterprise_custom_headers
