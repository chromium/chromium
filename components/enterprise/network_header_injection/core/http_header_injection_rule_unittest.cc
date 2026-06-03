// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"

#include <optional>
#include <utility>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_custom_headers {

TEST(HttpHeaderInjectionRuleTest, ParseValidValue) {
  base::DictValue rule_dict;

  base::ListValue patterns;
  patterns.Append("example.com");
  patterns.Append("google.com");
  rule_dict.Set(kKeyPatterns, std::move(patterns));

  base::ListValue headers;
  base::DictValue header1;
  header1.Set(kKeyHeaderName, "X-Header-1");
  header1.Set(kKeyHeaderValue, "Value1");
  headers.Append(std::move(header1));

  base::DictValue header2;
  header2.Set(kKeyHeaderName, "X-Header-2");
  header2.Set(kKeyHeaderValue, "Value2");
  headers.Append(std::move(header2));

  rule_dict.Set(kKeyHeaders, std::move(headers));

  std::optional<HttpHeaderInjectionRule> rule =
      HttpHeaderInjectionRule::FromValue(base::Value(std::move(rule_dict)));

  ASSERT_TRUE(rule.has_value());
  EXPECT_EQ(2u, rule->url_patterns.size());
  EXPECT_EQ("example.com", rule->url_patterns[0]);
  EXPECT_EQ("google.com", rule->url_patterns[1]);

  ASSERT_EQ(2u, rule->headers.size());
  EXPECT_EQ("X-Header-1", rule->headers[0].first);
  EXPECT_EQ("Value1", rule->headers[0].second);
  EXPECT_EQ("X-Header-2", rule->headers[1].first);
  EXPECT_EQ("Value2", rule->headers[1].second);
}

TEST(HttpHeaderInjectionRuleTest, ParseInvalidDict) {
  // Non-dict value should fail.
  base::Value non_dict(42);
  EXPECT_FALSE(HttpHeaderInjectionRule::FromValue(non_dict).has_value());

  // Empty dict should fail.
  base::DictValue empty_dict;
  EXPECT_FALSE(
      HttpHeaderInjectionRule::FromValue(base::Value(std::move(empty_dict)))
          .has_value());
}

TEST(HttpHeaderInjectionRuleTest, ParseMissingKeys) {
  base::DictValue rule_dict;

  // Missing headers list.
  base::ListValue patterns;
  patterns.Append("example.com");
  rule_dict.Set(kKeyPatterns, std::move(patterns));
  EXPECT_FALSE(
      HttpHeaderInjectionRule::FromValue(base::Value(rule_dict.Clone()))
          .has_value());

  // Missing patterns list.
  rule_dict.clear();
  base::ListValue headers;
  base::DictValue header;
  header.Set(kKeyHeaderName, "X-Header");
  header.Set(kKeyHeaderValue, "Value");
  headers.Append(std::move(header));
  rule_dict.Set(kKeyHeaders, std::move(headers));
  EXPECT_FALSE(
      HttpHeaderInjectionRule::FromValue(base::Value(std::move(rule_dict)))
          .has_value());
}

TEST(HttpHeaderInjectionRuleTest, ParseMalformedHeaders) {
  base::DictValue rule_dict;

  base::ListValue patterns;
  patterns.Append("example.com");
  rule_dict.Set(kKeyPatterns, std::move(patterns));

  // Headers list contains non-dict or dicts without name/value keys.
  base::ListValue headers;
  headers.Append(42);  // invalid type, should be skipped

  base::DictValue incomplete_header;
  incomplete_header.Set(kKeyHeaderName, "X-Header");  // missing value key
  headers.Append(std::move(incomplete_header));

  base::DictValue valid_header;
  valid_header.Set(kKeyHeaderName, "X-Test");
  valid_header.Set(kKeyHeaderValue, "Ok");
  headers.Append(std::move(valid_header));

  rule_dict.Set(kKeyHeaders, std::move(headers));

  std::optional<HttpHeaderInjectionRule> rule =
      HttpHeaderInjectionRule::FromValue(base::Value(std::move(rule_dict)));

  // The parsing should still succeed because there is at least one valid
  // header.
  ASSERT_TRUE(rule.has_value());
  ASSERT_EQ(1u, rule->headers.size());
  EXPECT_EQ("X-Test", rule->headers[0].first);
  EXPECT_EQ("Ok", rule->headers[0].second);
}

TEST(HttpHeaderInjectionRuleTest, ParseNoValidPatterns) {
  base::DictValue rule_dict;

  base::ListValue patterns;
  // Invalid pattern type.
  patterns.Append(42);
  rule_dict.Set(kKeyPatterns, std::move(patterns));

  base::ListValue headers;
  base::DictValue header;
  header.Set(kKeyHeaderName, "X-Test");
  header.Set(kKeyHeaderValue, "Value");
  headers.Append(std::move(header));
  rule_dict.Set(kKeyHeaders, std::move(headers));

  // Should fail because there are no valid patterns parsed.
  EXPECT_FALSE(
      HttpHeaderInjectionRule::FromValue(base::Value(std::move(rule_dict)))
          .has_value());
}

}  // namespace enterprise_custom_headers
