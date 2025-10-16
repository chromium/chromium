// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/selector.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace focus {

class SelectorTest : public testing::Test {
 public:
  SelectorTest() = default;
  ~SelectorTest() override = default;
};

TEST_F(SelectorTest, ParseSelectorsBasic) {
  std::vector<Selector> selectors = ParseSelectors("https://example.com");
  ASSERT_EQ(1u, selectors.size());
  EXPECT_EQ(SelectorType::kUrlExact, selectors[0].type);
  EXPECT_EQ("https://example.com/", selectors[0].url.spec());
}

TEST_F(SelectorTest, ParseSelectorsWildcard) {
  std::vector<Selector> selectors = ParseSelectors("https://example.com/*");
  ASSERT_EQ(1u, selectors.size());
  EXPECT_EQ(SelectorType::kUrlPrefix, selectors[0].type);
  EXPECT_EQ("https://example.com/", selectors[0].url.spec());
}

TEST_F(SelectorTest, ParseSelectorsMultiple) {
  std::vector<Selector> selectors =
      ParseSelectors("https://example.com,https://test.com/*");
  ASSERT_EQ(2u, selectors.size());

  EXPECT_EQ(SelectorType::kUrlExact, selectors[0].type);
  EXPECT_EQ("https://example.com/", selectors[0].url.spec());

  EXPECT_EQ(SelectorType::kUrlPrefix, selectors[1].type);
  EXPECT_EQ("https://test.com/", selectors[1].url.spec());
}

TEST_F(SelectorTest, ParseSelectorsInvalid) {
  // Empty string.
  EXPECT_TRUE(ParseSelectors("").empty());

  // Invalid URL.
  EXPECT_TRUE(ParseSelectors("not-a-url").empty());
}

TEST_F(SelectorTest, ParseSelectorsWhitespaceOnly) {
  // Whitespace-only input.
  EXPECT_TRUE(ParseSelectors("   ").empty());
  EXPECT_TRUE(ParseSelectors("\t\t").empty());
  EXPECT_TRUE(ParseSelectors(" \t \n ").empty());

  // Comma separated whitespace.
  EXPECT_TRUE(ParseSelectors(" , , ").empty());
  EXPECT_TRUE(ParseSelectors("\t,\t,\t").empty());
}

TEST_F(SelectorTest, ParseSelectorsMixedValidInvalid) {
  // Valid URL mixed with invalid.
  std::vector<Selector> selectors =
      ParseSelectors("https://example.com,not-a-url,https://valid.com");
  ASSERT_EQ(2u, selectors.size());

  EXPECT_EQ(SelectorType::kUrlExact, selectors[0].type);
  EXPECT_EQ("https://example.com/", selectors[0].url.spec());

  EXPECT_EQ(SelectorType::kUrlExact, selectors[1].type);
  EXPECT_EQ("https://valid.com/", selectors[1].url.spec());

  // Invalid URL mixed with valid URL.
  selectors = ParseSelectors("invalid-url,https://test.com/*,also-invalid");
  ASSERT_EQ(1u, selectors.size());

  EXPECT_EQ(SelectorType::kUrlPrefix, selectors[0].type);
  EXPECT_EQ("https://test.com/", selectors[0].url.spec());
}

TEST_F(SelectorTest, SelectorToString) {
  Selector url_selector(SelectorType::kUrlExact, GURL("https://example.com"));
  EXPECT_EQ("https://example.com/", url_selector.ToString());

  Selector prefix_selector(SelectorType::kUrlPrefix,
                           GURL("https://example.com"));
  EXPECT_EQ("https://example.com/*", prefix_selector.ToString());
}

TEST_F(SelectorTest, SelectorIsValid) {
  Selector valid_url(SelectorType::kUrlExact, GURL("https://example.com"));
  EXPECT_TRUE(valid_url.IsValid());

  Selector invalid_url(SelectorType::kUrlExact, GURL("not-a-url"));
  EXPECT_FALSE(invalid_url.IsValid());
}

}  // namespace focus
