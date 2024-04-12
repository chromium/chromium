// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/tabbed_mode_scope_matcher.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/liburlpattern/parse.h"
#include "url/gurl.h"

namespace {

std::vector<liburlpattern::Part> ParsePatternInitField(std::string_view field) {
  auto parse_result = liburlpattern::Parse(
      field, [](std::string_view input) { return std::string(input); });

  // Should never fail because the input is coming from the test.
  DCHECK(parse_result.ok());

  return parse_result.value().PartList();
}

TEST(TabbedModeScopeMatcher, Empty) {
  blink::SafeUrlPattern pattern;
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // All fields can match any value.
  EXPECT_TRUE(matcher.Match(
      GURL("http://user:pass@example.com:1234/foo/bar?x=y#anchor")));
}

TEST(TabbedModeScopeMatcher, Protocol) {
  blink::SafeUrlPattern pattern;
  pattern.protocol = ParsePatternInitField("http*");
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // Basic test.
  EXPECT_TRUE(matcher.Match(GURL("https://example.com/foo/bar")));

  // All other fields can match any value.
  EXPECT_TRUE(matcher.Match(
      GURL("http://user:pass@example.org:1234/foo/bar?x=y#anchor")));

  // Fail if the scheme does not match.
  EXPECT_FALSE(matcher.Match(GURL("ftp://example.com/foo/bar")));
}

TEST(TabbedModeScopeMatcher, Hostname) {
  blink::SafeUrlPattern pattern;
  pattern.hostname = ParsePatternInitField("example.*.com");
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // Basic test.
  EXPECT_TRUE(matcher.Match(GURL("https://example.a.com/foo/bar")));

  // All other fields can match any value.
  EXPECT_TRUE(matcher.Match(
      GURL("https://user:pass@example.b.com:1234/foo/bar?x=y#anchor")));

  // Fail if the hostname does not match.
  EXPECT_FALSE(matcher.Match(GURL("https://example.a.org/foo/bar")));
}

TEST(TabbedModeScopeMatcher, Port) {
  blink::SafeUrlPattern pattern;
  pattern.port = ParsePatternInitField("12*4");
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // Basic test.
  EXPECT_TRUE(matcher.Match(GURL("https://example.com:1234/foo/bar")));

  // All other fields can match any value.
  EXPECT_TRUE(matcher.Match(
      GURL("https://user:pass@example.com:12984/foo/bar?x=y#anchor")));

  // Fail if the port does not match.
  EXPECT_FALSE(matcher.Match(GURL("https://example.com:1233/foo/bar")));
}

TEST(TabbedModeScopeMatcher, Pathname) {
  blink::SafeUrlPattern pattern;
  pattern.pathname = ParsePatternInitField("/foo/*/bar");
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // Basic test.
  EXPECT_TRUE(matcher.Match(GURL("https://example.com/foo/x/bar")));

  // All other fields can match any value.
  EXPECT_TRUE(matcher.Match(
      GURL("https://user:pass@example.org:1234/foo/y/bar?x=y#anchor")));

  // Fail if the path does not match.
  EXPECT_FALSE(matcher.Match(GURL("https://example.com/foo/x/baz")));
}

TEST(TabbedModeScopeMatcher, Search) {
  blink::SafeUrlPattern pattern;
  pattern.search = ParsePatternInitField("x=*&p=q");
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // Basic test.
  EXPECT_TRUE(matcher.Match(GURL("https://example.com/foo/bar?x=y&p=q")));

  // All other fields can match any value.
  EXPECT_TRUE(matcher.Match(
      GURL("https://user:pass@example.com:1234/foo/bar?x=z&p=q#anchor")));

  // Fail if the search does not match.
  EXPECT_FALSE(matcher.Match(GURL("https://example.com/foo/bar?x=y&p=r")));

  // The order of query parameters matters (i.e. this is a simple string match).
  EXPECT_FALSE(matcher.Match(GURL("https://example.com/foo/bar?p=q&x=y")));
}

TEST(TabbedModeScopeMatcher, Hash) {
  blink::SafeUrlPattern pattern;
  pattern.hash = ParsePatternInitField("tr*t");
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // Basic test.
  EXPECT_TRUE(matcher.Match(GURL("https://example.com/foo/bar#treat")));

  // All other fields can match any value.
  EXPECT_TRUE(matcher.Match(
      GURL("https://user:pass@example.com:1234/foo/bar?x=y#trout")));

  // Fail if the search does not match.
  EXPECT_FALSE(matcher.Match(GURL("https://example.com/foo/bar#thought")));
}

TEST(TabbedModeScopeMatcher, All) {
  blink::SafeUrlPattern pattern;
  pattern.protocol = ParsePatternInitField("http*");
  pattern.username = ParsePatternInitField("us*r");
  pattern.password = ParsePatternInitField("pa*word");
  pattern.hostname = ParsePatternInitField("example.*.com");
  pattern.port = ParsePatternInitField("12*4");
  pattern.pathname = ParsePatternInitField("/foo/*/bar");
  pattern.search = ParsePatternInitField("x=*&p=q");
  pattern.hash = ParsePatternInitField("tr*t");
  web_app::TabbedModeScopeMatcher matcher(pattern);

  // Basic test.
  EXPECT_TRUE(matcher.Match(GURL(
      "https://user:password@example.a.com:1234/foo/x/bar?x=y&p=q#treat")));

  // Change all the fields but still match.
  EXPECT_TRUE(matcher.Match(GURL(
      "https://usir:pantword@example.b.com:12984/foo/y/bar?x=z&p=q#trout")));

  // If any one field doesn't match, the match fails.
  EXPECT_FALSE(matcher.Match(
      GURL("ftp://user:password@example.a.com:1234/foo/x/bar?x=y&p=q#treat")));
  EXPECT_FALSE(matcher.Match(GURL(
      "https://uber:password@example.a.com:1234/foo/x/bar?x=y&p=q#treat")));
  EXPECT_FALSE(matcher.Match(GURL(
      "https://user:passworn@example.a.com:1234/foo/x/bar?x=y&p=q#treat")));
  EXPECT_FALSE(matcher.Match(GURL(
      "https://user:password@example.a.org:1234/foo/x/bar?x=y&p=q#treat")));
  EXPECT_FALSE(matcher.Match(GURL(
      "https://user:password@example.a.com:1233/foo/x/bar?x=y&p=q#treat")));
  EXPECT_FALSE(matcher.Match(GURL(
      "https://user:password@example.a.com:1234/foo/x/baz?x=y&p=q#treat")));
  EXPECT_FALSE(matcher.Match(GURL(
      "https://user:password@example.a.com:1234/foo/x/bar?x=y&p=r#treat")));
  EXPECT_FALSE(matcher.Match(GURL(
      "https://user:password@example.a.com:1234/foo/x/bar?x=y&p=q#thought")));
}

}  // namespace
