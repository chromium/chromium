// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/string_cleaning.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace omnibox {

TEST(StringCleaningTest, CleanUpUrlForMatching) {
  EXPECT_EQ(u"http://foo.com/", CleanUpUrlForMatching(GURL("http://foo.com"),
                                                      /*adjustments=*/nullptr));
  EXPECT_EQ(u"http://foo.com/", CleanUpUrlForMatching(GURL("http://Foo.com"),
                                                      /*adjustments=*/nullptr));
}

TEST(StringCleaningTest, CleanUpUrlForMatchingShortUrlSkipsTruncation) {
  // A URL well under the 1024-char limit should produce the same result
  // whether or not the truncation optimization is active. This exercises the
  // fast path that reuses the existing GURL without re-parsing.
  const GURL short_url("http://www.example.com/path?q=hello#frag");
  std::u16string result =
      CleanUpUrlForMatching(short_url, /*adjustments=*/nullptr);
  EXPECT_FALSE(result.empty());
  // Result should be lowercased.
  EXPECT_EQ(result, u"http://www.example.com/path?q=hello#frag");
}

TEST(StringCleaningTest, CleanUpUrlForMatchingLongUrlTruncates) {
  // Build a URL longer than 1024 characters to exercise the truncation path.
  std::string long_path(1100, 'a');
  std::string long_url_str = "http://www.example.com/" + long_path;
  const GURL long_url(long_url_str);
  ASSERT_TRUE(long_url.is_valid());
  std::u16string result =
      CleanUpUrlForMatching(long_url, /*adjustments=*/nullptr);
  // Result should be truncated — shorter than the original.
  EXPECT_LE(result.length(), 1024u + 20u);  // Allow for formatting overhead.
  EXPECT_FALSE(result.empty());
}

TEST(StringCleaningTest, CleanUpUrlForMatchingWithCredentials) {
  // URL with username/password — should be omitted by
  // kFormatUrlOmitUsernamePassword.
  const GURL url_with_creds("http://user:pass@www.example.com/");
  std::u16string result =
      CleanUpUrlForMatching(url_with_creds, /*adjustments=*/nullptr);
  // Credentials should be stripped.
  EXPECT_EQ(result.find(u"user"), std::u16string::npos);
  EXPECT_EQ(result.find(u"pass"), std::u16string::npos);
  EXPECT_NE(result.find(u"www.example.com"), std::u16string::npos);
}

TEST(StringCleaningTest, CleanUpEmptyUrlForMatching) {
  GURL empty_url("");
  EXPECT_DCHECK_DEATH(
      CleanUpUrlForMatching(empty_url, /*adjustments=*/nullptr));
}

TEST(StringCleaningTest, CleanUpTitleForMatching) {
  // Test basic title cleaning
  std::u16string title1 = u"Example Title";
  std::u16string result1 = CleanUpTitleForMatching(title1);
  EXPECT_EQ(u"example title", result1);

  // Test long title truncation
  std::u16string long_title(2000, u'A');
  std::u16string result3 = CleanUpTitleForMatching(long_title);
  EXPECT_EQ(1024u, result3.length());

  // Test empty title
  std::u16string empty_title;
  std::u16string empty_result = CleanUpTitleForMatching(empty_title);
  EXPECT_TRUE(empty_result.empty());
}

}  // namespace omnibox
