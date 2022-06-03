// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

size_t strlen16(const char16_t* str) {
  return std::char_traits<char16_t>::length(str);
}

std::vector<base::StringPiece16> StringsToStringPieces(
    const std::vector<std::u16string>& strings) {
  std::vector<base::StringPiece16> string_pieces;
  for (const auto& s : strings) {
    string_pieces.emplace_back(base::StringPiece16(s));
  }
  return string_pieces;
}
}  // namespace

namespace autofill {

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

// Tests that the correct length of prefixes and suffixes are returned.
TEST(NameProcessingUtil, FindLongestCommonAffixLength) {
  auto String16ToStringPiece16 = [](std::vector<std::u16string>& vin,
                                    std::vector<base::StringPiece16>& vout) {
    vout.clear();
    for (auto& str : vin)
      vout.push_back(str);
  };

  // Normal prefix case.
  std::vector<std::u16string> strings;
  std::vector<base::StringPiece16> stringPieces;
  strings.push_back(u"123456XXX123456789");
  strings.push_back(u"12345678XXX012345678_foo");
  strings.push_back(u"1234567890123456");
  strings.push_back(u"1234567XXX901234567890");
  String16ToStringPiece16(strings, stringPieces);
  size_t affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(strlen16(u"123456"), affixLength);

  // Normal suffix case.
  strings.clear();
  strings.push_back(u"black and gold dress");
  strings.push_back(u"work_address");
  strings.push_back(u"123456XXX1234_home_address");
  strings.push_back(u"1234567890123456_city_address");
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(strlen16(u"dress"), affixLength);

  // Handles no common prefix.
  strings.clear();
  strings.push_back(u"1234567890123456");
  strings.push_back(u"4567890123456789");
  strings.push_back(u"7890123456789012");
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(strlen16(u""), affixLength);

  // Handles no common suffix.
  strings.clear();
  strings.push_back(u"1234567890123456");
  strings.push_back(u"4567890123456789");
  strings.push_back(u"7890123456789012");
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(strlen16(u""), affixLength);

  // Only one string, prefix case.
  strings.clear();
  strings.push_back(u"1234567890");
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(strlen16(u"1234567890"), affixLength);

  // Only one string, suffix case.
  strings.clear();
  strings.push_back(u"1234567890");
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(strlen16(u"1234567890"), affixLength);

  // Empty vector, prefix case.
  strings.clear();
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(strlen16(u""), affixLength);

  // Empty vector, suffix case.
  strings.clear();
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(strlen16(u""), affixLength);
}

// Tests the determination of the length of the longest common prefix for
// strings with a minimal length.
TEST(NameProcessingUtil,
     FindLongestCommonPrefixLengthForStringsWithMinimalLength) {
  std::vector<std::u16string> strings;
  strings.push_back(u"aabbccddeeff");
  strings.push_back(u"aabbccddeeffgg");
  strings.push_back(u"zzz");
  strings.push_back(u"aabbc___");
  EXPECT_EQ(FindLongestCommonPrefixLengthInStringsWithMinimalLength(
                StringsToStringPieces(strings), 4),
            5U);
  EXPECT_EQ(FindLongestCommonPrefixLengthInStringsWithMinimalLength(
                StringsToStringPieces(strings), 3),
            0U);
}

// Tests that a |absl::nullopt| is returned if no common affix was removed.
TEST(NameProcessingUtil, RemoveCommonAffixesIfPossible_NotPossible) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abc");
  strings.push_back(u"def");
  strings.push_back(u"abcd");
  strings.push_back(u"abcdef");

  EXPECT_EQ(RemoveCommonAffixesIfPossible(StringsToStringPieces(strings)),
            absl::nullopt);
}

// Tests that both the prefix and the suffix are removed.
TEST(NameProcessingUtil, RemoveCommonAffixesIfPossible) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abcaazzz");
  strings.push_back(u"abcbbzzz");
  strings.push_back(u"abccczzz");

  std::vector<std::u16string> expectation;
  expectation.push_back(u"aa");
  expectation.push_back(u"bb");
  expectation.push_back(u"cc");

  EXPECT_EQ(RemoveCommonAffixesIfPossible(StringsToStringPieces(strings)),
            StringsToStringPieces(expectation));
}

// Tests that a |absl::nullopt| is returned if no common prefix was removed.
TEST(NameProcessingUtil, RemoveCommonPrefixIfPossible_NotPossible) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abc");
  strings.push_back(u"def");
  strings.push_back(u"abcd");
  strings.push_back(u"abcdef");

  EXPECT_EQ(RemoveCommonPrefixIfPossible(StringsToStringPieces(strings)),
            absl::nullopt);
}

// Tests that prefix is removed correctly.
TEST(NameProcessingUtil, RemoveCommonPrefixIfPossible) {
  std::vector<std::u16string> strings;
  // The strings contain a long common prefix that can be removed.
  strings.push_back(u"ccccccccccccccccaazzz");
  strings.push_back(u"ccccccccccccccccbbzzz");
  strings.push_back(u"cccccccccccccccccczzz");

  std::vector<std::u16string> expectation;
  expectation.push_back(u"aazzz");
  expectation.push_back(u"bbzzz");
  expectation.push_back(u"cczzz");

  EXPECT_EQ(RemoveCommonPrefixIfPossible(StringsToStringPieces(strings)),
            StringsToStringPieces(expectation));
}

// Tests that prefix is removed correctly for fields with a minimal length.
TEST(NameProcessingUtil,
     RemoveCommonPrefixForFieldsWithMinimalLengthIfPossible) {
  std::vector<std::u16string> strings;
  strings.push_back(u"ccccccccccccccccaazzz");
  // This name is too short to be considered and is skipped both in the
  // detection of prefixes as well as in the removal.
  strings.push_back(u"abc");
  strings.push_back(u"cccccccccccccccccczzz");

  std::vector<std::u16string> expectation;
  expectation.push_back(u"aazzz");
  expectation.push_back(u"abc");
  expectation.push_back(u"cczzz");

  EXPECT_EQ(RemoveCommonPrefixForNamesWithMinimalLengthIfPossible(
                StringsToStringPieces(strings)),
            StringsToStringPieces(expectation));
}

// Tests that prefix is not removed because it is too short.
TEST(NameProcessingUtil, RemoveCommonPrefixIfPossible_TooShort) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abcaazzz");
  strings.push_back(u"abcbbzzz");
  strings.push_back(u"abccczzz");

  EXPECT_EQ(RemoveCommonPrefixIfPossible(StringsToStringPieces(strings)),
            absl::nullopt);
}

// Tests that the strings are correctly stripped.
TEST(NameProcessingUtil, GetStrippedParseableNamesIfValid) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abcaazzz");
  strings.push_back(u"abcbbzzz");
  strings.push_back(u"abccczzz");

  std::vector<std::u16string> expectation;
  expectation.push_back(u"aaz");
  expectation.push_back(u"bbz");
  expectation.push_back(u"ccz");

  EXPECT_EQ(
      GetStrippedParseableNamesIfValid(StringsToStringPieces(strings), 3, 2, 1),
      StringsToStringPieces(expectation));
}

// Tests that a |absl::nullopt| is returned if one of stripped names is not
// valid.
TEST(NameProcessingUtil, GetStrippedParseableNamesIfValid_NotValid) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abcaazzz");
  // This string is not valid because only the "1" is left after stripping.
  strings.push_back(u"abc1zz");
  strings.push_back(u"abccczzz");

  std::vector<std::u16string> expectation;
  expectation.push_back(u"aaz");
  expectation.push_back(u"bbz");
  expectation.push_back(u"ccz");

  EXPECT_EQ(
      GetStrippedParseableNamesIfValid(StringsToStringPieces(strings), 3, 2, 1),
      absl::nullopt);
}

// Tests that the parseable names are returned correctly.
TEST(NameProcessingUtil, GetParseableNames_OnlyPrefix) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abcaazzz1");
  strings.push_back(u"abcbbzzz2");
  strings.push_back(u"abccczzz3");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned on, the prefix is removed.
  std::vector<std::u16string> expectation;
  expectation.push_back(u"aazzz1");
  expectation.push_back(u"bbzzz2");
  expectation.push_back(u"cczzz3");

  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);
}

// Tests that the parseable names are returned correctly.
TEST(NameProcessingUtil, GetParseableNames_OnlySuffix) {
  std::vector<std::u16string> strings;
  strings.push_back(u"1aazzz");
  strings.push_back(u"2bbzzz");
  strings.push_back(u"3cczzz");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned on, the suffix is removed.
  std::vector<std::u16string> expectation;
  expectation.push_back(u"1aa");
  expectation.push_back(u"2bb");
  expectation.push_back(u"3cc");

  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);
}

// Tests that the parseable names are returned correctly.
TEST(NameProcessingUtil, GetParseableNames_Affix) {
  std::vector<std::u16string> strings;
  strings.push_back(u"abcaazzz");
  strings.push_back(u"abcbbzzz");
  strings.push_back(u"abccczzz");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned on, the prefix and affix is removed.
  std::vector<std::u16string> expectation;
  expectation.push_back(u"aa");
  expectation.push_back(u"bb");
  expectation.push_back(u"cc");

  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);

  scoped_feature_list.Reset();
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned off, the names are too short for a prefix removal.
  expectation.clear();
  expectation.push_back(u"abcaazzz");
  expectation.push_back(u"abcbbzzz");
  expectation.push_back(u"abccczzz");
  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);

  // But very long prefixes are still removed.
  strings.clear();
  strings.push_back(u"1234567890ABCDEFGabcaazzz");
  strings.push_back(u"1234567890ABCDEFGabcbbzzz");
  strings.push_back(u"1234567890ABCDEFGabccczzz");

  expectation.clear();
  expectation.push_back(u"aazzz");
  expectation.push_back(u"bbzzz");
  expectation.push_back(u"cczzz");
  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);
}
}  // namespace autofill
