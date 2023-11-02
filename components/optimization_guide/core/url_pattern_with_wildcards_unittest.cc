// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/url_pattern_with_wildcards.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(URLPatternWithWildcardsTest, OnePattern) {
  URLPatternWithWildcards url_pattern_with_wildcards("foo.jpg");

  const struct {
    std::string url;
    bool expect_matches;
  } tests[] = {
      {"https://www.example.com/", false},
      {"https://www.example.com/foo.js", false},
      {"https://www.example.com/foo.jpg", true},
      {"https://www.example.com/pages/foo.jpg", true},
      {"https://www.example.com/foobar.jpg", false},
      {"https://www.example.com/barfoo.jpg", true},
      {"http://www.example.com/foo.jpg", true},
      {"http://www.example.com/foo.jpg?q=alpha", true},
      {"http://www.example.com/bar.jpg?q=foo.jpg", true},
      {"http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg", true},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expect_matches,
              url_pattern_with_wildcards.Matches(test.url));
  }
}

TEST(URLPatternWithWildcardsTest, OnePatternWithOneWildcard) {
  URLPatternWithWildcards url_pattern_with_wildcards(
      "example.com/bar/*/foo.jpg");

  const struct {
    std::string url;
    bool expect_matches;
  } tests[] = {
      {"https://www.example.com/", false},
      {"https://www.example.com/foo.js", false},
      {"https://www.example.com/foo.jpg", false},
      {"https://www.example.com/pages/foo.jpg", false},
      {"https://www.example.com/foobar.jpg", false},
      {"https://www.example.com/barfoo.jpg", false},
      {"http://www.example.com/foo.jpg", false},
      {"http://www.example.com/foo.jpg?q=alpha", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg", false},
      {"https://www.example.com/bar/foo.jpg", false},
      {"https://www.example.com/bar/pages/foo.jpg", true},
      {"https://www.example.com/bar/main_page/foo.jpg", true},
      {"https://www.example.com/bar/pages/subpages/foo.jpg", true},
      // Try different prefixes.
      {"https://m.example.com/bar/main_page/foo.jpg", true},
      {"https://in.example.com/bar/main_page/foo.jpg", true},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expect_matches,
              url_pattern_with_wildcards.Matches(test.url));
  }
}

TEST(URLPatternWithWildcardsTest, OnePatternWithOneWildcardAtEnds) {
  URLPatternWithWildcards url_pattern_with_wildcards("*example.com/bar/*");

  const struct {
    std::string url;
    bool expect_matches;
  } tests[] = {
      {"https://www.example.com/", false},
      {"https://www.example.com/foo.js", false},
      {"https://www.example.com/foo.jpg", false},
      {"https://www.example.com/pages/foo.jpg", false},
      {"https://www.example.com/foobar.jpg", false},
      {"https://www.example.com/barfoo.jpg", false},
      {"http://www.example.com/foo.jpg", false},
      {"http://www.example.com/foo.jpg?q=alpha", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg", false},
      {"https://www.example.com/bar/foo.jpg", true},
      {"https://www.example.com/bar/pages/foo.jpg", true},
      {"https://www.example.com/bar/main_page/foo.jpg", true},
      {"https://www.example.com/bar/pages/subpages/foo.jpg", true},
      // Try different prefixes.
      {"https://m.example.com/bar/main_page/foo.jpg", true},
      {"https://in.example.com/bar/main_page/foo.jpg", true},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expect_matches,
              url_pattern_with_wildcards.Matches(test.url));
  }
}

TEST(URLPatternWithWildcardsTest, OnePatternWithOneWildcardAndScheme) {
  URLPatternWithWildcards url_pattern_with_wildcards(
      "https://www.example.com/bar/*/foo.jpg");

  const struct {
    std::string url;
    bool expect_matches;
  } tests[] = {
      {"https://www.example.com/", false},
      {"https://www.example.com/foo.js", false},
      {"https://www.example.com/foo.jpg", false},
      {"https://www.example.com/pages/foo.jpg", false},
      {"https://www.example.com/foobar.jpg", false},
      {"https://www.example.com/barfoo.jpg", false},
      {"http://www.example.com/foo.jpg", false},
      {"http://www.example.com/foo.jpg?q=alpha", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg", false},
      {"https://www.example.com/bar/foo.jpg", false},
      {"https://www.example.com/bar/pages/foo.jpg", true},
      {"https://www.example.com/bar/main_page/foo.jpg", true},
      {"https://www.example.com/bar/pages/subpages/foo.jpg", true},
      // Different scheme.
      {"http://www.example.com/bar/pages/foo.jpg", false},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expect_matches,
              url_pattern_with_wildcards.Matches(test.url));
  }
}

TEST(URLPatternWithWildcardsTest, OnePatternWithMultipleWildcards) {
  URLPatternWithWildcards url_pattern_with_wildcards(
      "example.com/bar/*/pages/*/*.jpg");

  const struct {
    std::string url;
    bool expect_matches;
  } tests[] = {
      {"https://www.example.com/", false},
      {"https://www.example.com/foo.js", false},
      {"https://www.example.com/foo.jpg", false},
      {"https://www.example.com/pages/foo.jpg", false},
      {"https://www.example.com/foobar.jpg", false},
      {"https://www.example.com/barfoo.jpg", false},
      {"http://www.example.com/foo.jpg", false},
      {"http://www.example.com/foo.jpg?q=alpha", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg", false},
      {"https://www.example.com/bar/foo.jpg", false},
      {"https://www.example.com/bar/pages/foo.jpg", false},
      {"https://www.example.com/bar/main_page/foo.jpg", false},
      {"https://www.example.com/bar/pages/subpages/foo.jpg", false},
      {"https://www.example.com/bar/main/pages/document/foo.jpg", true},
      {"https://www.example.com/bar/main/pages/document/foo1.jpg", true},
      {"https://www.example.com/bar/main/pages/document/foo1.js", false},
      // Out-of-order subpatterns.
      {"https://cdn.com/pages/www.example.com/bar/document/foo.jpg", false},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expect_matches,
              url_pattern_with_wildcards.Matches(test.url));
  }
}

TEST(URLPatternWithWildcardsTest,
     OnePatternWithMultipleWildcardsImplicitSuffix) {
  URLPatternWithWildcards url_pattern_with_wildcards(
      "example.com/bar/*/pages/");

  const struct {
    std::string url;
    bool expect_matches;
  } tests[] = {
      {"https://www.example.com/", false},
      {"https://www.example.com/foo.js", false},
      {"https://www.example.com/foo.jpg", false},
      {"https://www.example.com/pages/foo.jpg", false},
      {"https://www.example.com/foobar.jpg", false},
      {"https://www.example.com/barfoo.jpg", false},
      {"http://www.example.com/foo.jpg", false},
      {"http://www.example.com/foo.jpg?q=alpha", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg", false},
      {"http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg", false},
      {"https://www.example.com/bar/foo.jpg", false},
      // No gap between "bar" and "pages".
      {"https://www.example.com/bar/pages/foo.jpg", false},
      {"https://www.example.com/bar/main_page/foo.jpg", false},
      // No gap between "bar" and "pages".
      {"https://www.example.com/bar/pages/subpages/foo.jpg", false},
      {"https://www.example.com/bar/main/pages/document/foo.jpg", true},
      {"https://www.example.com/bar/main/pages/document/foo1.jpg", true},
      {"https://www.example.com/bar/main/pages/document/foo1.js", true},
      // Out-of-order subpatterns.
      {"https://cdn.com/pages/www.example.com/bar/document/foo.jpg", false},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expect_matches, url_pattern_with_wildcards.Matches(test.url))
        << " url=" << test.url;
  }
}

}  // namespace

}  // namespace optimization_guide
