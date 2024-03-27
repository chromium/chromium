// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using testing::ElementsAre;

// Tests that the length of the longest common prefix is computed correctly.
TEST(NameProcessingUtil, FindLongestCommonAffixLength) {
  std::vector<std::u16string_view> strings = {
      u"123456XXX123456789", u"12345678XXX012345678_foo", u"1234567890123456",
      u"1234567XXX901234567890"};
  EXPECT_EQ(std::string_view("123456").size(),
            FindLongestCommonAffixLength(strings, /*prefix=*/true));
  strings = {u"1234567890"};
  EXPECT_EQ(std::string_view("1234567890").size(),
            FindLongestCommonAffixLength(strings, /*prefix=*/true));
  strings = {u"1234567890123456", u"4567890123456789", u"7890123456789012"};
  EXPECT_EQ(0u, FindLongestCommonAffixLength(strings, /*prefix=*/true));
  strings = {};
  EXPECT_EQ(0u, FindLongestCommonAffixLength(strings, /*prefix=*/true));
  strings = {u"a123", u"b123", u"c123"};
  EXPECT_EQ(std::string_view("123").size(),
            FindLongestCommonAffixLength(strings,
                                         /*prefix=*/false));
}

// Tests that the parseable names are computed correctly.
TEST(NameProcessingUtil, ComputeParseableNames) {
  // No common prefix.
  std::vector<std::u16string_view> no_common_prefix = {u"abc", u"def", u"abcd",
                                                       u"abcdef"};
  ComputeParseableNames(no_common_prefix);
  EXPECT_THAT(no_common_prefix,
              ElementsAre(u"abc", u"def", u"abcd", u"abcdef"));

  // The prefix is too short to be removed.
  std::vector<std::u16string_view> short_prefix = {u"abcaazzz", u"abcbbzzz",
                                                   u"abccczzz"};
  ComputeParseableNames(short_prefix);
  EXPECT_THAT(short_prefix, ElementsAre(u"abcaazzz", u"abcbbzzz", u"abccczzz"));

  // Not enough strings to be considered for prefix removal.
  std::vector<std::u16string_view> not_enough_strings = {
      u"ccccccccccccccccaazzz", u"ccccccccccccccccbbzzz"};
  ComputeParseableNames(not_enough_strings);
  EXPECT_THAT(not_enough_strings,
              ElementsAre(u"ccccccccccccccccaazzz", u"ccccccccccccccccbbzzz"));

  // Long prefixes are removed.
  std::vector<std::u16string_view> long_prefix = {u"1234567890ABCDEFGabcaazzz",
                                                  u"1234567890ABCDEFGabcbbzzz",
                                                  u"1234567890ABCDEFGabccczzz"};
  ComputeParseableNames(long_prefix);
  EXPECT_THAT(long_prefix, ElementsAre(u"aazzz", u"bbzzz", u"cczzz"));
}

}  // namespace autofill
