// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include <string_view>

#include "base/containers/to_vector.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

// Tests that the length of the longest common prefix is computed correctly.
TEST(NameProcessingUtil, FindLongestCommonAffixLength) {
  std::vector<std::u16string_view> strings = {
      u"123456XXX123456789", u"12345678XXX012345678_foo", u"1234567890123456",
      u"1234567XXX901234567890"};
  EXPECT_EQ(std::string_view("123456").size(),
            FindLongestCommonAffixLengthForTest(strings, /*prefix=*/true));
  strings = {u"1234567890"};
  EXPECT_EQ(std::string_view("1234567890").size(),
            FindLongestCommonAffixLengthForTest(strings, /*prefix=*/true));
  strings = {u"1234567890123456", u"4567890123456789", u"7890123456789012"};
  EXPECT_EQ(0u, FindLongestCommonAffixLengthForTest(strings, /*prefix=*/true));
  strings = {};
  EXPECT_EQ(0u, FindLongestCommonAffixLengthForTest(strings, /*prefix=*/true));
  strings = {u"a123", u"b123", u"c123"};
  EXPECT_EQ(std::string_view("123").size(),
            FindLongestCommonAffixLengthForTest(strings,
                                                /*prefix=*/false));
}

// Tests that the parseable names are computed correctly.
TEST(NameProcessingUtil, ComputeParseableNames) {
  // No common prefix.
  std::vector<std::u16string_view> no_common_prefix = {u"abc", u"def", u"abcd",
                                                       u"abcdef"};
  ComputeParseableNamesForTest(no_common_prefix);
  EXPECT_THAT(no_common_prefix,
              ElementsAre(u"abc", u"def", u"abcd", u"abcdef"));

  // The prefix is too short to be removed.
  std::vector<std::u16string_view> short_prefix = {u"abcaazzz", u"abcbbzzz",
                                                   u"abccczzz"};
  ComputeParseableNamesForTest(short_prefix);
  EXPECT_THAT(short_prefix, ElementsAre(u"abcaazzz", u"abcbbzzz", u"abccczzz"));

  // Not enough strings to be considered for prefix removal.
  std::vector<std::u16string_view> not_enough_strings = {
      u"ccccccccccccccccaazzz", u"ccccccccccccccccbbzzz"};
  ComputeParseableNamesForTest(not_enough_strings);
  EXPECT_THAT(not_enough_strings,
              ElementsAre(u"ccccccccccccccccaazzz", u"ccccccccccccccccbbzzz"));

  // Long prefixes are removed.
  std::vector<std::u16string_view> long_prefix = {u"1234567890ABCDEFGabcaazzz",
                                                  u"1234567890ABCDEFGabcbbzzz",
                                                  u"1234567890ABCDEFGabccczzz"};
  ComputeParseableNamesForTest(long_prefix);
  EXPECT_THAT(long_prefix, ElementsAre(u"aazzz", u"bbzzz", u"cczzz"));
}

// Tests that GetParseableNames() returns parseable names if they differ from
// the original names.
TEST(NameProcessingUtil, GetParseableNamesWithCommonPrefix) {
  test::AutofillUnitTestEnvironment autofill_environment;
  std::vector<std::unique_ptr<AutofillField>> fields;
  for (const char* name : {"1234567890ABCDEF_Foo", "1234567890ABCDEF_Bar",
                           "1234567890ABCDEF_Qux"}) {
    fields.push_back(std::make_unique<AutofillField>(test::CreateTestFormField(
        /*label=*/"", /*name=*/name,
        /*value=*/"", /*type=*/FormControlType::kInputText)));
  }
  EXPECT_THAT(GetParseableNames(fields),
              UnorderedElementsAre(Pair(fields[0]->global_id(), u"Foo"),
                                   Pair(fields[1]->global_id(), u"Bar"),
                                   Pair(fields[2]->global_id(), u"Qux")));
}

// Tests that GetParseableNames() does not return parseable names that are
// identical to the original names.
TEST(NameProcessingUtil, GetParseableNamesWithoutCommonPrefix) {
  test::AutofillUnitTestEnvironment autofill_environment;
  std::vector<std::unique_ptr<AutofillField>> fields;
  for (const char* name : {"Foo", "Bar", "Qux"}) {
    fields.push_back(std::make_unique<AutofillField>(test::CreateTestFormField(
        /*label=*/"", /*name=*/name,
        /*value=*/"", /*type=*/FormControlType::kInputText)));
  }
  EXPECT_THAT(GetParseableNames(fields), IsEmpty());
}

}  // namespace
}  // namespace autofill
