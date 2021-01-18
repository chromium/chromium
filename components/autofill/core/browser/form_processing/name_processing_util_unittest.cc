// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {
std::vector<base::StringPiece16> StringsToStringPieces(
    const std::vector<base::string16>& strings) {
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
  EXPECT_FALSE(IsValidParseableName(ASCIIToUTF16("")));
  // Parseable name should not be solely numerical.
  EXPECT_FALSE(IsValidParseableName(ASCIIToUTF16("1265125")));

  // Valid parseable name cases.
  EXPECT_TRUE(IsValidParseableName(ASCIIToUTF16("a23")));
  EXPECT_TRUE(IsValidParseableName(ASCIIToUTF16("*)&%@")));
}

// Tests that the correct length of prefixes and suffixes are returned.
TEST(NameProcessingUtil, FindLongestCommonAffixLength) {
  auto String16ToStringPiece16 = [](std::vector<base::string16>& vin,
                                    std::vector<base::StringPiece16>& vout) {
    vout.clear();
    for (auto& str : vin)
      vout.push_back(str);
  };

  // Normal prefix case.
  std::vector<base::string16> strings;
  std::vector<base::StringPiece16> stringPieces;
  strings.push_back(ASCIIToUTF16("123456XXX123456789"));
  strings.push_back(ASCIIToUTF16("12345678XXX012345678_foo"));
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("1234567XXX901234567890"));
  String16ToStringPiece16(strings, stringPieces);
  size_t affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("123456").size(), affixLength);

  // Normal suffix case.
  strings.clear();
  strings.push_back(ASCIIToUTF16("black and gold dress"));
  strings.push_back(ASCIIToUTF16("work_address"));
  strings.push_back(ASCIIToUTF16("123456XXX1234_home_address"));
  strings.push_back(ASCIIToUTF16("1234567890123456_city_address"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("dress").size(), affixLength);

  // Handles no common prefix.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("4567890123456789"));
  strings.push_back(ASCIIToUTF16("7890123456789012"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);

  // Handles no common suffix.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("4567890123456789"));
  strings.push_back(ASCIIToUTF16("7890123456789012"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);

  // Only one string, prefix case.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("1234567890").size(), affixLength);

  // Only one string, suffix case.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("1234567890").size(), affixLength);

  // Empty vector, prefix case.
  strings.clear();
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);

  // Empty vector, suffix case.
  strings.clear();
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);
}

// Tests the determination of the length of the longest common prefix for
// strings with a minimal length.
TEST(NameProcessingUtil,
     FindLongestCommonPrefixLengthForStringsWithMinimalLength) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("aabbccddeeff"));
  strings.push_back(ASCIIToUTF16("aabbccddeeffgg"));
  strings.push_back(ASCIIToUTF16("zzz"));
  strings.push_back(ASCIIToUTF16("aabbc___"));
  EXPECT_EQ(FindLongestCommonPrefixLengthInStringsWithMinimalLength(
                StringsToStringPieces(strings), 4),
            5U);
  EXPECT_EQ(FindLongestCommonPrefixLengthInStringsWithMinimalLength(
                StringsToStringPieces(strings), 3),
            0U);
}

// Tests that a |base::nullopt| is returned if no common affix was removed.
TEST(NameProcessingUtil, RemoveCommonAffixesIfPossible_NotPossible) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abc"));
  strings.push_back(ASCIIToUTF16("def"));
  strings.push_back(ASCIIToUTF16("abcd"));
  strings.push_back(ASCIIToUTF16("abcdef"));

  EXPECT_EQ(RemoveCommonAffixesIfPossible(StringsToStringPieces(strings)),
            base::nullopt);
}

// Tests that both the prefix and the suffix are removed.
TEST(NameProcessingUtil, RemoveCommonAffixesIfPossible) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abcaazzz"));
  strings.push_back(ASCIIToUTF16("abcbbzzz"));
  strings.push_back(ASCIIToUTF16("abccczzz"));

  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("aa"));
  expectation.push_back(ASCIIToUTF16("bb"));
  expectation.push_back(ASCIIToUTF16("cc"));

  EXPECT_EQ(RemoveCommonAffixesIfPossible(StringsToStringPieces(strings)),
            StringsToStringPieces(expectation));
}

// Tests that a |base::nullopt| is returned if no common prefix was removed.
TEST(NameProcessingUtil, RemoveCommonPrefixIfPossible_NotPossible) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abc"));
  strings.push_back(ASCIIToUTF16("def"));
  strings.push_back(ASCIIToUTF16("abcd"));
  strings.push_back(ASCIIToUTF16("abcdef"));

  EXPECT_EQ(RemoveCommonPrefixIfPossible(StringsToStringPieces(strings)),
            base::nullopt);
}

// Tests that prefix is removed correctly.
TEST(NameProcessingUtil, RemoveCommonPrefixIfPossible) {
  std::vector<base::string16> strings;
  // The strings contain a long common prefix that can be removed.
  strings.push_back(ASCIIToUTF16("ccccccccccccccccaazzz"));
  strings.push_back(ASCIIToUTF16("ccccccccccccccccbbzzz"));
  strings.push_back(ASCIIToUTF16("cccccccccccccccccczzz"));

  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("aazzz"));
  expectation.push_back(ASCIIToUTF16("bbzzz"));
  expectation.push_back(ASCIIToUTF16("cczzz"));

  EXPECT_EQ(RemoveCommonPrefixIfPossible(StringsToStringPieces(strings)),
            StringsToStringPieces(expectation));
}

// Tests that prefix is removed correctly for fields with a minimal length.
TEST(NameProcessingUtil,
     RemoveCommonPrefixForFieldsWithMinimalLengthIfPossible) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("ccccccccccccccccaazzz"));
  // This name is too short to be considered and is skipped both in the
  // detection of prefixes as well as in the removal.
  strings.push_back(ASCIIToUTF16("abc"));
  strings.push_back(ASCIIToUTF16("cccccccccccccccccczzz"));

  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("aazzz"));
  expectation.push_back(ASCIIToUTF16("abc"));
  expectation.push_back(ASCIIToUTF16("cczzz"));

  EXPECT_EQ(RemoveCommonPrefixForNamesWithMinimalLengthIfPossible(
                StringsToStringPieces(strings)),
            StringsToStringPieces(expectation));
}

// Tests that prefix is not removed because it is too short.
TEST(NameProcessingUtil, RemoveCommonPrefixIfPossible_TooShort) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abcaazzz"));
  strings.push_back(ASCIIToUTF16("abcbbzzz"));
  strings.push_back(ASCIIToUTF16("abccczzz"));

  EXPECT_EQ(RemoveCommonPrefixIfPossible(StringsToStringPieces(strings)),
            base::nullopt);
}

// Tests that the strings are correctly stripped.
TEST(NameProcessingUtil, GetStrippedParseableNamesIfValid) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abcaazzz"));
  strings.push_back(ASCIIToUTF16("abcbbzzz"));
  strings.push_back(ASCIIToUTF16("abccczzz"));

  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("aaz"));
  expectation.push_back(ASCIIToUTF16("bbz"));
  expectation.push_back(ASCIIToUTF16("ccz"));

  EXPECT_EQ(
      GetStrippedParseableNamesIfValid(StringsToStringPieces(strings), 3, 2, 1),
      StringsToStringPieces(expectation));
}

// Tests that a |base::nullopt| is returned if one of stripped names is not
// valid.
TEST(NameProcessingUtil, GetStrippedParseableNamesIfValid_NotValid) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abcaazzz"));
  // This string is not valid because only the "1" is left after stripping.
  strings.push_back(ASCIIToUTF16("abc1zz"));
  strings.push_back(ASCIIToUTF16("abccczzz"));

  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("aaz"));
  expectation.push_back(ASCIIToUTF16("bbz"));
  expectation.push_back(ASCIIToUTF16("ccz"));

  EXPECT_EQ(
      GetStrippedParseableNamesIfValid(StringsToStringPieces(strings), 3, 2, 1),
      base::nullopt);
}

// Tests that the parseable names are returned correctly.
TEST(NameProcessingUtil, GetParseableNames_OnlyPrefix) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abcaazzz1"));
  strings.push_back(ASCIIToUTF16("abcbbzzz2"));
  strings.push_back(ASCIIToUTF16("abccczzz3"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned on, the prefix is removed.
  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("aazzz1"));
  expectation.push_back(ASCIIToUTF16("bbzzz2"));
  expectation.push_back(ASCIIToUTF16("cczzz3"));

  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);
}

// Tests that the parseable names are returned correctly.
TEST(NameProcessingUtil, GetParseableNames_OnlySuffix) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("1aazzz"));
  strings.push_back(ASCIIToUTF16("2bbzzz"));
  strings.push_back(ASCIIToUTF16("3cczzz"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned on, the suffix is removed.
  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("1aa"));
  expectation.push_back(ASCIIToUTF16("2bb"));
  expectation.push_back(ASCIIToUTF16("3cc"));

  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);
}

// Tests that the parseable names are returned correctly.
TEST(NameProcessingUtil, GetParseableNames_Affix) {
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("abcaazzz"));
  strings.push_back(ASCIIToUTF16("abcbbzzz"));
  strings.push_back(ASCIIToUTF16("abccczzz"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned on, the prefix and affix is removed.
  std::vector<base::string16> expectation;
  expectation.push_back(ASCIIToUTF16("aa"));
  expectation.push_back(ASCIIToUTF16("bb"));
  expectation.push_back(ASCIIToUTF16("cc"));

  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);

  scoped_feature_list.Reset();
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillLabelAffixRemoval);

  // With the feature turned off, the names are too short for a prefix removal.
  expectation.clear();
  expectation.push_back(ASCIIToUTF16("abcaazzz"));
  expectation.push_back(ASCIIToUTF16("abcbbzzz"));
  expectation.push_back(ASCIIToUTF16("abccczzz"));
  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);

  // But very long prefixes are still removed.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890ABCDEFGabcaazzz"));
  strings.push_back(ASCIIToUTF16("1234567890ABCDEFGabcbbzzz"));
  strings.push_back(ASCIIToUTF16("1234567890ABCDEFGabccczzz"));

  expectation.clear();
  expectation.push_back(ASCIIToUTF16("aazzz"));
  expectation.push_back(ASCIIToUTF16("bbzzz"));
  expectation.push_back(ASCIIToUTF16("cczzz"));
  EXPECT_EQ(GetParseableNames(StringsToStringPieces(strings)), expectation);
}
}  // namespace autofill
