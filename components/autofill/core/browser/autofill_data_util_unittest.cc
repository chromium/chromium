// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace data_util {
namespace {

using data_util::bit_field_type_groups::kAddress;
using data_util::bit_field_type_groups::kEmail;
using data_util::bit_field_type_groups::kName;
using data_util::bit_field_type_groups::kPhone;

TEST(AutofillDataUtilTest, DetermineGroupsForHomeNameAndAddress) {
  const std::vector<ServerFieldType> field_types{
      NAME_FIRST,        NAME_LAST,          ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP};

  const uint32_t expected_group_bitmask = kName | kAddress;
  const uint32_t group_bitmask = data_util::DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(AutofillDataUtilTest, DetermineGroupsForBillingNameAndAddress) {
  const std::vector<ServerFieldType> field_types{
      NAME_BILLING_FULL, ADDRESS_BILLING_LINE1, ADDRESS_BILLING_CITY,
      ADDRESS_BILLING_STATE, ADDRESS_BILLING_ZIP};

  const uint32_t expected_group_bitmask = kName | kAddress;
  const uint32_t group_bitmask = data_util::DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(AutofillDataUtilTest, DetermineGroupsForHomeNamePhoneAndEmail) {
  const std::vector<ServerFieldType> field_types{
      NAME_FULL, PHONE_HOME_CITY_AND_NUMBER, EMAIL_ADDRESS};

  const uint32_t expected_group_bitmask = kName | kPhone | kEmail;
  const uint32_t group_bitmask = data_util::DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(AutofillDataUtilTest, DetermineGroupsForBillingNamePhoneAndEmail) {
  const std::vector<ServerFieldType> field_types{
      NAME_BILLING_FULL, PHONE_BILLING_WHOLE_NUMBER, EMAIL_ADDRESS};

  const uint32_t expected_group_bitmask = kName | kPhone | kEmail;
  const uint32_t group_bitmask = data_util::DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(AutofillDataUtilTest, DetermineGroupsForUnknownServerFieldType) {
  const std::vector<ServerFieldType> field_types{UNKNOWN_TYPE, NAME_FULL,
                                                 ADDRESS_HOME_ZIP};

  const uint32_t expected_group_bitmask = kName | kAddress;
  const uint32_t group_bitmask = data_util::DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(AutofillDataUtilTest, DetermineGroupsForNoServerFieldTypes) {
  const std::vector<ServerFieldType> field_types =
      std::vector<ServerFieldType>();
  const uint32_t expected_group_bitmask = 0;
  const uint32_t group_bitmask = data_util::DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

struct IsCJKNameTestCase {
  const char* full_name;
  bool is_cjk;
};

class IsCJKNameTest : public testing::TestWithParam<IsCJKNameTestCase> {};

TEST_P(IsCJKNameTest, IsCJKName) {
  auto test_case = GetParam();
  EXPECT_EQ(test_case.is_cjk, IsCJKName(base::UTF8ToUTF16(test_case.full_name)))
      << "Failed for: " << test_case.full_name;
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataUtilTest,
    IsCJKNameTest,
    testing::Values(
        // Non-CJK language with only ASCII characters.
        IsCJKNameTestCase{"Homer Jay Simpson", false},
        // Non-CJK language with some ASCII characters.
        IsCJKNameTestCase{"Éloïse Paré", false},
        // Non-CJK language with no ASCII characters.
        IsCJKNameTestCase{"Σωκράτης", false},

        // (Simplified) Chinese name, Unihan.
        IsCJKNameTestCase{"刘翔", true},
        // (Simplified) Chinese name, Unihan, with an ASCII space.
        IsCJKNameTestCase{"成 龙", true},
        // Korean name, Hangul.
        IsCJKNameTestCase{"송지효", true},
        // Korean name, Hangul, with an 'IDEOGRAPHIC SPACE' (U+3000).
        IsCJKNameTestCase{"김　종국", true},
        // Japanese name, Unihan.
        IsCJKNameTestCase{"山田貴洋", true},
        // Japanese name, Katakana, with a 'KATAKANA MIDDLE DOT' (U+30FB).
        IsCJKNameTestCase{"ビル・ゲイツ", true},
        // Japanese name, Katakana, with a 'MIDDLE DOT' (U+00B7) (likely a
        // typo).
        IsCJKNameTestCase{"ビル·ゲイツ", true},

        // CJK names don't have a middle name, so a 3-part name is bogus to us.
        IsCJKNameTestCase{"반 기 문", false}));

struct FullNameTestCase {
  std::string full_name;
  std::string given_name;
  std::string middle_name;
  std::string family_name;
};

class SplitNameTest : public testing::TestWithParam<FullNameTestCase> {};

TEST_P(SplitNameTest, SplitName) {
  auto test_case = GetParam();
  NameParts name_parts = SplitName(base::UTF8ToUTF16(test_case.full_name));

  EXPECT_EQ(base::UTF8ToUTF16(test_case.given_name), name_parts.given);
  EXPECT_EQ(base::UTF8ToUTF16(test_case.middle_name), name_parts.middle);
  EXPECT_EQ(base::UTF8ToUTF16(test_case.family_name), name_parts.family);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataUtil,
    SplitNameTest,
    testing::Values(
        // Full name including given, middle and family names.
        FullNameTestCase{"Homer Jay Simpson", "Homer", "Jay", "Simpson"},
        // No middle name.
        FullNameTestCase{"Moe Szyslak", "Moe", "", "Szyslak"},
        // Common name prefixes removed.
        FullNameTestCase{"Reverend Timothy Lovejoy", "Timothy", "", "Lovejoy"},
        // Common name suffixes removed.
        FullNameTestCase{"John Frink Phd", "John", "", "Frink"},
        // Exception to the name suffix removal.
        FullNameTestCase{"John Ma", "John", "", "Ma"},
        // Common family name prefixes not considered a middle name.
        FullNameTestCase{"Milhouse Van Houten", "Milhouse", "", "Van Houten"},

        // CJK names have reverse order (surname goes first, given name goes
        // second).
        FullNameTestCase{"孫 德明", "德明", "", "孫"},  // Chinese name, Unihan
        FullNameTestCase{"孫　德明", "德明", "",
                         "孫"},  // Chinese name, Unihan, 'IDEOGRAPHIC SPACE'
        FullNameTestCase{"홍 길동", "길동", "", "홍"},  // Korean name, Hangul
        FullNameTestCase{"山田 貴洋", "貴洋", "",
                         "山田"},  // Japanese name, Unihan

        // In Japanese, foreign names use 'KATAKANA MIDDLE DOT' (U+30FB) as a
        // separator. There is no consensus for the ordering. For now, we use
        // the same ordering as regular Japanese names ("last・first").
        FullNameTestCase{"ゲイツ・ビル", "ビル", "",
                         "ゲイツ"},  // Foreign name in Japanese, Katakana
        // 'KATAKANA MIDDLE DOT' is occasionally typoed as 'MIDDLE DOT'
        // (U+00B7).
        FullNameTestCase{"ゲイツ·ビル", "ビル", "",
                         "ゲイツ"},  // Foreign name in Japanese, Katakana

        // CJK names don't usually have a space in the middle, but most of the
        // time, the surname is only one character (in Chinese & Korean).
        FullNameTestCase{"최성훈", "성훈", "", "최"},  // Korean name, Hangul
        FullNameTestCase{"刘翔", "翔", "",
                         "刘"},  // (Simplified) Chinese name, Unihan
        FullNameTestCase{"劉翔", "翔", "",
                         "劉"},  // (Traditional) Chinese name, Unihan

        // There are a few exceptions. Occasionally, the surname has two
        // characters.
        FullNameTestCase{"남궁도", "도", "", "남궁"},  // Korean name, Hangul
        FullNameTestCase{"황보혜정", "혜정", "",
                         "황보"},  // Korean name, Hangul
        FullNameTestCase{"歐陽靖", "靖", "",
                         "歐陽"},  // (Traditional) Chinese name, Unihan

        // In Korean, some 2-character surnames are rare/ambiguous, like "강전":
        // "강" is a common surname, and "전" can be part of a given name. In
        // those cases, we assume it's 1/2 for 3-character names, or 2/2 for
        // 4-character names.
        FullNameTestCase{"강전희", "전희", "", "강"},  // Korean name, Hangul
        FullNameTestCase{"황목치승", "치승", "",
                         "황목"},  // Korean name, Hangul

        // It occasionally happens that a full name is 2 characters, 1/1.
        FullNameTestCase{"이도", "도", "", "이"},  // Korean name, Hangul
        FullNameTestCase{"孫文", "文", "", "孫"}   // Chinese name, Unihan
        ));

class JoinNamePartsTest : public testing::TestWithParam<FullNameTestCase> {};

TEST_P(JoinNamePartsTest, JoinNameParts) {
  auto test_case = GetParam();
  base::string16 joined =
      JoinNameParts(base::UTF8ToUTF16(test_case.given_name),
                    base::UTF8ToUTF16(test_case.middle_name),
                    base::UTF8ToUTF16(test_case.family_name));

  EXPECT_EQ(base::UTF8ToUTF16(test_case.full_name), joined);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataUtil,
    JoinNamePartsTest,
    testing::Values(
        // Full name including given, middle and family names.
        FullNameTestCase{"Homer Jay Simpson", "Homer", "Jay", "Simpson"},
        // No middle name.
        FullNameTestCase{"Moe Szyslak", "Moe", "", "Szyslak"},

        // CJK names have reversed order, no space.
        FullNameTestCase{"孫德明", "德明", "", "孫"},  // Chinese name, Unihan
        FullNameTestCase{"홍길동", "길동", "", "홍"},  // Korean name, Hangul
        FullNameTestCase{"山田貴洋", "貴洋", "",
                         "山田"},  // Japanese name, Unihan

        // These are no CJK names for us, they're just bogus.
        FullNameTestCase{"Homer シンプソン", "Homer", "", "シンプソン"},
        FullNameTestCase{"ホーマー Simpson", "ホーマー", "", "Simpson"},
        FullNameTestCase{"반 기 문", "반", "기", "문"}
        // Has a middle-name, too unusual
        ));

struct ValidCountryCodeTestCase {
  std::string country_code;
  bool expected_result;
};

class ValidCountryCodeTest
    : public testing::TestWithParam<ValidCountryCodeTestCase> {};

TEST_P(ValidCountryCodeTest, ValidCountryCode) {
  auto test_case = GetParam();
  EXPECT_EQ(test_case.expected_result,
            IsValidCountryCode(test_case.country_code));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataUtil,
    ValidCountryCodeTest,
    testing::Values(
        // Valid country codes.
        ValidCountryCodeTestCase{"US", true},
        ValidCountryCodeTestCase{"CA", true},
        ValidCountryCodeTestCase{"CN", true},

        // Country names should not be considered valid.
        ValidCountryCodeTestCase{"United States", false},
        ValidCountryCodeTestCase{"Canada", false},
        ValidCountryCodeTestCase{"China", false},

        // Codes with numbers should not be considered valid.
        ValidCountryCodeTestCase{"C2", false},

        // Three letters abbreviations should not be considered valid.
        ValidCountryCodeTestCase{"USA", false},
        ValidCountryCodeTestCase{"CAN", false},
        ValidCountryCodeTestCase{"CHN", false},

        // Lowercase is invalid.
        ValidCountryCodeTestCase{"us", false},
        ValidCountryCodeTestCase{"Ca", false},
        ValidCountryCodeTestCase{"cN", false}));

}  // namespace
}  // namespace data_util
}  // namespace autofill
