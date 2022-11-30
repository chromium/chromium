// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using testing::ElementsAre;

// Tests that the validity of parseable names is determined correctly.
TEST(NameProcessingUtil, IsValidParseableName) {
  // Parseable name should not be empty.
  EXPECT_FALSE(IsValidParseableName(u""));
  // Parseable name should not be solely numerical.
  EXPECT_FALSE(IsValidParseableName(u"1265125"));
  // Valid parseable name cases.
  EXPECT_TRUE(IsValidParseableName(u"a23"));
  EXPECT_TRUE(IsValidParseableName(u"*)&%@"));
}

// Tests that the length of the longest common prefix is computed correctly.
TEST(NameProcessingUtil, FindLongestCommonPrefixLength) {
  EXPECT_EQ(base::StringPiece("123456").size(),
            FindLongestCommonPrefixLength(
                {u"123456XXX123456789", u"12345678XXX012345678_foo",
                 u"1234567890123456", u"1234567XXX901234567890"}));
  EXPECT_EQ(base::StringPiece("1234567890").size(),
            FindLongestCommonPrefixLength({u"1234567890"}));
  EXPECT_EQ(
      0u, FindLongestCommonPrefixLength(
              {u"1234567890123456", u"4567890123456789", u"7890123456789012"}));
  EXPECT_EQ(0u, FindLongestCommonPrefixLength({}));
}

TEST(NameProcessingUtil, RemoveCommonPrefixIfPossible) {
  // No common prefix.
  EXPECT_FALSE(
      RemoveCommonPrefixIfPossible({u"abc", u"def", u"abcd", u"abcdef"}));
  // The common prefix is too short.
  EXPECT_FALSE(
      RemoveCommonPrefixIfPossible({u"abcaazzz", u"abcbbzzz", u"abccczzz"}));
  // Not enough strings.
  EXPECT_FALSE(RemoveCommonPrefixIfPossible(
      {u"ccccccccccccccccaazzz", u"ccccccccccccccccbbzzz"}));
  // A long common prefix of enough strings is removed.
  EXPECT_THAT(RemoveCommonPrefixIfPossible({u"ccccccccccccccccaazzz",
                                            u"ccccccccccccccccbbzzz",
                                            u"cccccccccccccccccczzz"}),
              testing::Optional(ElementsAre(u"aazzz", u"bbzzz", u"cczzz")));
}

// Tests that the parseable names are returned correctly.
TEST(NameProcessingUtil, GetParseableNames) {
  // The prefix is too short, so the original strings are returned.
  std::vector<base::StringPiece16> short_prefix{u"abcaazzz", u"abcbbzzz",
                                                u"abccczzz"};
  EXPECT_THAT(GetParseableNamesAsStringPiece(&short_prefix),
              testing::ElementsAreArray(short_prefix));
  // Long prefixes are removed.
  std::vector<base::StringPiece16> long_prefix{u"1234567890ABCDEFGabcaazzz",
                                               u"1234567890ABCDEFGabcbbzzz",
                                               u"1234567890ABCDEFGabccczzz"};
  EXPECT_THAT(GetParseableNamesAsStringPiece(&long_prefix),
              ElementsAre(u"aazzz", u"bbzzz", u"cczzz"));
}
}  // namespace autofill
