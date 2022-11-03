// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using testing::ElementsAre;

// Tests that the length of the longest common prefix is computed correctly.
TEST(NameProcessingUtil, FindLongestCommonPrefixLength) {
  std::vector<base::StringPiece16> strings = {
      u"123456XXX123456789", u"12345678XXX012345678_foo", u"1234567890123456",
      u"1234567XXX901234567890"};
  EXPECT_EQ(base::StringPiece("123456").size(),
            FindLongestCommonPrefixLength(strings));
  strings = {u"1234567890"};
  EXPECT_EQ(base::StringPiece("1234567890").size(),
            FindLongestCommonPrefixLength(strings));
  strings = {u"1234567890123456", u"4567890123456789", u"7890123456789012"};
  EXPECT_EQ(0u, FindLongestCommonPrefixLength(strings));
  strings = {};
  EXPECT_EQ(0u, FindLongestCommonPrefixLength(strings));
}

// Tests that the parseable names are computed correctly.
TEST(NameProcessingUtil, ComputeParseableNames) {
  // No common prefix.
  std::vector<base::StringPiece16> no_common_prefix = {u"abc", u"def", u"abcd",
                                                       u"abcdef"};
  ComputeParseableNames(no_common_prefix);
  EXPECT_THAT(no_common_prefix,
              ElementsAre(u"abc", u"def", u"abcd", u"abcdef"));

  // The prefix is too short to be removed.
  std::vector<base::StringPiece16> short_prefix = {u"abcaazzz", u"abcbbzzz",
                                                   u"abccczzz"};
  ComputeParseableNames(short_prefix);
  EXPECT_THAT(short_prefix, ElementsAre(u"abcaazzz", u"abcbbzzz", u"abccczzz"));

  // Not enough strings to be considered for prefix removal.
  std::vector<base::StringPiece16> not_enough_strings = {
      u"ccccccccccccccccaazzz", u"ccccccccccccccccbbzzz"};
  ComputeParseableNames(not_enough_strings);
  EXPECT_THAT(not_enough_strings,
              ElementsAre(u"ccccccccccccccccaazzz", u"ccccccccccccccccbbzzz"));

  // Long prefixes are removed.
  std::vector<base::StringPiece16> long_prefix = {u"1234567890ABCDEFGabcaazzz",
                                                  u"1234567890ABCDEFGabcbbzzz",
                                                  u"1234567890ABCDEFGabccczzz"};
  ComputeParseableNames(long_prefix);
  EXPECT_THAT(long_prefix, ElementsAre(u"aazzz", u"bbzzz", u"cczzz"));
}
}  // namespace autofill
