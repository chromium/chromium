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
