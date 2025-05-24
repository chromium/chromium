// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/contact_info.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

struct FullNameTestCase {
  std::string full_name_input;
  std::string given_name_output;
  std::string middle_name_output;
  std::string family_name_output;
};

struct FullAlternativeNameTestCase {
  std::u16string full_name_input;
  std::u16string given_name_output;
  std::u16string family_name_output;
};

class SetFullNameTest : public testing::TestWithParam<FullNameTestCase> {};

TEST_P(SetFullNameTest, SetFullName) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.full_name_input);

  NameInfo name;
  name.SetInfo(NAME_FULL, ASCIIToUTF16(test_case.full_name_input), "en-US");
  EXPECT_TRUE(name.FinalizeAfterImport());
  EXPECT_EQ(ASCIIToUTF16(test_case.given_name_output),
            name.GetInfo(NAME_FIRST, "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.middle_name_output),
            name.GetInfo(NAME_MIDDLE, "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.family_name_output),
            name.GetInfo(NAME_LAST, "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.full_name_input),
            name.GetInfo(NAME_FULL, "en-US"));
}

class SetFullAlternativeNameTest
    : public testing::TestWithParam<FullAlternativeNameTestCase> {};

TEST_P(SetFullAlternativeNameTest, SetFullAlternativeName) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSupportPhoneticNameForJP);

  auto test_case = GetParam();
  SCOPED_TRACE(test_case.full_name_input);

  NameInfo name;
  name.SetInfo(ALTERNATIVE_FULL_NAME, test_case.full_name_input, "ja");
  EXPECT_TRUE(name.FinalizeAfterImport());
  EXPECT_EQ(test_case.given_name_output,
            name.GetInfo(ALTERNATIVE_GIVEN_NAME, "ja"));
  EXPECT_EQ(test_case.family_name_output,
            name.GetInfo(ALTERNATIVE_FAMILY_NAME, "ja"));
  EXPECT_EQ(test_case.full_name_input,
            name.GetInfo(ALTERNATIVE_FULL_NAME, "ja"));
}

INSTANTIATE_TEST_SUITE_P(
    SetFullAlternativeName,
    SetFullAlternativeNameTest,
    testing::Values(
        FullAlternativeNameTestCase{u"", u"", u""},
        FullAlternativeNameTestCase{u"John Smith", u"John", u"Smith"},
        FullAlternativeNameTestCase{u"やまもと あおい", u"あおい", u"やまもと"},
        FullAlternativeNameTestCase{u"さとう さくら", u"さくら", u"さとう"},
        FullAlternativeNameTestCase{u"たなか・あおい", u"あおい", u"たなか"},
        FullAlternativeNameTestCase{u"スズキ-エミ", u"エミ", u"スズキ"}));

class NameInfoTest : public testing::Test {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillSupportPhoneticNameForJP};
};

TEST_F(NameInfoTest, GetMatchingTypes) {
  NameInfo name;

  test::FormGroupValues name_values = {
      {.type = NAME_FULL,
       .value = "Pablo Diego Ruiz y Picasso",
       .verification_status = VerificationStatus::kObserved}};
  test::SetFormGroupValues(name, name_values);
  name.FinalizeAfterImport();

  test::FormGroupValues expectation = {
      {.type = NAME_FIRST,
       .value = "Pablo Diego",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_MIDDLE,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST,
       .value = "Ruiz y Picasso",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_FIRST,
       .value = "Ruiz",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_SECOND,
       .value = "Picasso",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_CONJUNCTION,
       .value = "y",
       .verification_status = VerificationStatus::kParsed}};

  test::VerifyFormGroupValues(name, expectation);

  FieldTypeSet matching_types;
  name.GetMatchingTypes(u"Ruiz", "US", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({NAME_LAST_FIRST}));

  // The honorific prefix is ignored.
  name.GetMatchingTypes(u"Mr.", "US", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({NAME_LAST_FIRST}));
}

// Tests the scenario in which the profile with an alternative name is correctly
// finalized after import.
TEST_F(NameInfoTest, FinalizeAfterImportWithAlternativeName) {
  NameInfo name;

  test::FormGroupValues name_values = {
      {.type = NAME_FULL,
       .value = "山本 葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_FULL_NAME,
       .value = "やまもと あおい",
       .verification_status = VerificationStatus::kObserved}};
  test::SetFormGroupValues(name, name_values);
  name.FinalizeAfterImport();

  test::FormGroupValues expectation = {
      {.type = NAME_FULL,
       .value = "山本 葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = NAME_FIRST,
       .value = "葵",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_MIDDLE,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST,
       .value = "山本",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_FIRST,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_SECOND,
       .value = "山本",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_CONJUNCTION,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = ALTERNATIVE_FULL_NAME,
       .value = "やまもと あおい",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_GIVEN_NAME,
       .value = "あおい",
       .verification_status = VerificationStatus::kParsed},
      {.type = ALTERNATIVE_FAMILY_NAME,
       .value = "やまもと",
       .verification_status = VerificationStatus::kParsed}};

  test::VerifyFormGroupValues(name, expectation);

  // Both "あおい" and "アオイ" are semantically equal.
  FieldTypeSet matching_types;
  name.GetMatchingTypes(u"あおい", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_GIVEN_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"アオイ", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_GIVEN_NAME}));
  matching_types.clear();

  // Both "やまもと" and "ヤマモト" are semantically equal.
  name.GetMatchingTypes(u"やまもと", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FAMILY_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"ヤマモト", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FAMILY_NAME}));
}

// Tests the scenario in which the profile with full name and alternative family
// name is correctly finalized after import.
TEST_F(NameInfoTest, FinalizeAfterImportWithNameAndIncompleteAlternativeName1) {
  NameInfo name;

  test::FormGroupValues name_values = {
      {.type = NAME_FULL,
       .value = "山本 葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_FAMILY_NAME,
       .value = "やまもと",
       .verification_status = VerificationStatus::kObserved}};
  test::SetFormGroupValues(name, name_values);
  name.FinalizeAfterImport();

  test::FormGroupValues expectation = {
      {.type = NAME_FULL,
       .value = "山本 葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = NAME_FIRST,
       .value = "葵",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_MIDDLE,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST,
       .value = "山本",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_FIRST,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_SECOND,
       .value = "山本",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_CONJUNCTION,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = ALTERNATIVE_FULL_NAME,
       .value = "やまもと",
       .verification_status = VerificationStatus::kFormatted},
      {.type = ALTERNATIVE_GIVEN_NAME,
       .value = "",
       .verification_status = VerificationStatus::kNoStatus},
      {.type = ALTERNATIVE_FAMILY_NAME,
       .value = "やまもと",
       .verification_status = VerificationStatus::kObserved}};

  test::VerifyFormGroupValues(name, expectation);

  FieldTypeSet matching_types;
  name.GetMatchingTypes(u"葵", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({NAME_FIRST}));
  matching_types.clear();

  // Both "やまもと" and "ヤマモト" are semantically equal.
  name.GetMatchingTypes(u"やまもと", "JP", &matching_types);
  EXPECT_EQ(matching_types,
            FieldTypeSet({ALTERNATIVE_FAMILY_NAME, ALTERNATIVE_FULL_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"ヤマモト", "JP", &matching_types);
  EXPECT_EQ(matching_types,
            FieldTypeSet({ALTERNATIVE_FAMILY_NAME, ALTERNATIVE_FULL_NAME}));

  // ALTERNATIVE_GIVEN_NAME does not get set.
  EXPECT_THAT(name.GetRawInfo(ALTERNATIVE_GIVEN_NAME), testing::IsEmpty());
}

// Tests the scenario in which the profile without an alternative full name
// is correctly finalized after import.
TEST_F(NameInfoTest, FinalizeAfterImportWithNameAndIncompleteAlternativeName2) {
  NameInfo name;

  test::FormGroupValues name_values = {
      {.type = NAME_FULL,
       .value = "山本 葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_GIVEN_NAME,
       .value = "あおい",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_FAMILY_NAME,
       .value = "やまもと",
       .verification_status = VerificationStatus::kObserved}};
  test::SetFormGroupValues(name, name_values);
  name.FinalizeAfterImport();

  test::FormGroupValues expectation = {
      {.type = NAME_FIRST,
       .value = "葵",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_FULL,
       .value = "山本 葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "山本",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_FIRST,
       .value = "",
       .verification_status = VerificationStatus::kParsed},
      {.type = NAME_LAST_SECOND,
       .value = "山本",
       .verification_status = VerificationStatus::kParsed},
      {.type = ALTERNATIVE_FULL_NAME,
       .value = "やまもと あおい",
       .verification_status = VerificationStatus::kFormatted},
      {.type = ALTERNATIVE_GIVEN_NAME,
       .value = "あおい",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_FAMILY_NAME,
       .value = "やまもと",
       .verification_status = VerificationStatus::kObserved}};

  test::VerifyFormGroupValues(name, expectation);

  FieldTypeSet matching_types;
  // Both "やまもと あおい" and "ヤマモト アオイ" are semantically equal.
  name.GetMatchingTypes(u"やまもと あおい", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FULL_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"ヤマモト アオイ", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FULL_NAME}));
}

// Tests the scenario in which the profile with an incomplete structured name
// is correctly finalized after import.
TEST_F(NameInfoTest, FinalizeAfterImportWithIncompleteNameAndAlternativeName) {
  NameInfo name;

  test::FormGroupValues name_values = {
      {.type = NAME_FIRST,
       .value = "葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_FULL_NAME,
       .value = "やまもと あおい",
       .verification_status = VerificationStatus::kObserved}};
  test::SetFormGroupValues(name, name_values);
  name.FinalizeAfterImport();

  test::FormGroupValues expectation = {
      {.type = NAME_FULL,
       .value = "葵",
       .verification_status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "葵",
       .verification_status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "",
       .verification_status = VerificationStatus::kFormatted},
      {.type = NAME_LAST_FIRST,
       .value = "",
       .verification_status = VerificationStatus::kNoStatus},
      {.type = NAME_LAST_SECOND,
       .value = "",
       .verification_status = VerificationStatus::kNoStatus},
      {.type = ALTERNATIVE_FULL_NAME,
       .value = "やまもと あおい",
       .verification_status = VerificationStatus::kObserved},
      {.type = ALTERNATIVE_GIVEN_NAME,
       .value = "あおい",
       .verification_status = VerificationStatus::kParsed},
      {.type = ALTERNATIVE_FAMILY_NAME,
       .value = "やまもと",
       .verification_status = VerificationStatus::kParsed}};

  test::VerifyFormGroupValues(name, expectation);

  FieldTypeSet matching_types;
  name.GetMatchingTypes(u"葵", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({NAME_FULL, NAME_FIRST}));
  matching_types.clear();

  // Both "やまもと あおい" and "ヤマモト アオイ" are semantically equal.
  name.GetMatchingTypes(u"あおい", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_GIVEN_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"アオイ", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_GIVEN_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"やまもと", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FAMILY_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"ヤマモト", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FAMILY_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"やまもと あおい", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FULL_NAME}));
  matching_types.clear();

  name.GetMatchingTypes(u"ヤマモト アオイ", "JP", &matching_types);
  EXPECT_EQ(matching_types, FieldTypeSet({ALTERNATIVE_FULL_NAME}));
}

// Tests the scenario in which the structured name is merged.
TEST_F(NameInfoTest, MergeStructuredName) {
  NameInfo name1;
  test::SetFormGroupValues(name1, {{.type = NAME_FULL, .value = "John Doe"}});

  NameInfo name2;
  test::SetFormGroupValues(name2, {{.type = NAME_FIRST, .value = "John"},
                                   {.type = NAME_FULL, .value = "John Doe"},
                                   {.type = NAME_LAST, .value = "Doe"}});

  EXPECT_TRUE(name1.MergeStructuredName(name2));

  test::VerifyFormGroupValues(name1, {{.type = NAME_FULL, .value = "John Doe"},
                                      {.type = NAME_FIRST, .value = "John"},
                                      {.type = NAME_LAST, .value = "Doe"}});
}

// Tests the scenario in which the alternative name is merged.
TEST_F(NameInfoTest, MergeStructuredAlternativeName) {
  NameInfo stored_profile;
  test::SetFormGroupValues(
      stored_profile,
      {{.type = ALTERNATIVE_FAMILY_NAME, .value = "やまもと"},
       {.type = ALTERNATIVE_FULL_NAME, .value = "やまもと あおい"}});

  NameInfo submitted_data;
  test::SetFormGroupValues(
      submitted_data,
      {{.type = ALTERNATIVE_GIVEN_NAME, .value = "あおい"},
       {.type = ALTERNATIVE_FAMILY_NAME, .value = "やまもと"},
       {.type = ALTERNATIVE_FULL_NAME, .value = "やまもと あおい"}});

  EXPECT_TRUE(stored_profile.MergeStructuredName(submitted_data));

  test::VerifyFormGroupValues(
      stored_profile,
      {{.type = ALTERNATIVE_GIVEN_NAME, .value = "あおい"},
       {.type = ALTERNATIVE_FAMILY_NAME, .value = "やまもと"},
       {.type = ALTERNATIVE_FULL_NAME, .value = "やまもと あおい"}});
}

// Tests the scenario in which both the structured name and the alternative
// name are merged.
TEST_F(NameInfoTest, MergeStructuredNameMergingBoth) {
  NameInfo stored_profile;
  test::SetFormGroupValues(
      stored_profile, {{.type = NAME_FULL, .value = "John Doe"},
                       {.type = ALTERNATIVE_FULL_NAME, .value = "John Doe"}});

  NameInfo submitted_data;
  test::SetFormGroupValues(
      submitted_data, {{.type = NAME_LAST, .value = "Doe"},
                       {.type = NAME_FIRST, .value = "John"},
                       {.type = NAME_FULL, .value = "John Doe"},
                       {.type = ALTERNATIVE_GIVEN_NAME, .value = "John"},
                       {.type = ALTERNATIVE_FAMILY_NAME, .value = "Doe"},
                       {.type = ALTERNATIVE_FULL_NAME, .value = "John Doe"}});

  EXPECT_TRUE(stored_profile.MergeStructuredName(submitted_data));

  test::VerifyFormGroupValues(
      stored_profile, {{.type = NAME_LAST, .value = "Doe"},
                       {.type = NAME_FIRST, .value = "John"},
                       {.type = NAME_FULL, .value = "John Doe"},
                       {.type = ALTERNATIVE_GIVEN_NAME, .value = "John"},
                       {.type = ALTERNATIVE_FAMILY_NAME, .value = "Doe"}});
}

INSTANTIATE_TEST_SUITE_P(
    ContactInfoTest,
    SetFullNameTest,
    testing::Values(
        FullNameTestCase{"", "", "", ""},
        FullNameTestCase{"John Smith", "John", "", "Smith"},
        FullNameTestCase{"Julien van der Poel", "Julien", "", "van der Poel"},
        FullNameTestCase{"John J Johnson", "John", "J", "Johnson"},
        FullNameTestCase{"John Smith, Jr.", "John", "", "Smith"},
        FullNameTestCase{"Mr John Smith", "John", "", "Smith"},
        FullNameTestCase{"Mr. John Smith", "John", "", "Smith"},
        FullNameTestCase{"Mr. John Smith, M.D.", "John", "", "Smith"},
        FullNameTestCase{"Mr. John Smith, MD", "John", "", "Smith"},
        FullNameTestCase{"Mr. John Smith MD", "John", "", "Smith"},
        FullNameTestCase{"William Hubert J.R.", "William", "Hubert", "J.R."},
        FullNameTestCase{"John Ma", "John", "", "Ma"},
        FullNameTestCase{"John Jacob Jingleheimer Smith", "John",
                         "Jacob Jingleheimer", "Smith"},
        FullNameTestCase{"Virgil", "", "", "Virgil"},
        FullNameTestCase{"Murray Gell-Mann", "Murray", "", "Gell-Mann"},
        FullNameTestCase{"Mikhail Yevgrafovich Saltykov-Shchedrin", "Mikhail",
                         "Yevgrafovich", "Saltykov-Shchedrin"},
        FullNameTestCase{"Arthur Ignatius Conan Doyle", "Arthur",
                         "Ignatius Conan", "Doyle"},
        FullNameTestCase{"Pablo Diego Ruiz y Picasso", "Pablo Diego", "",
                         "Ruiz y Picasso"}));

TEST(CompanyTest, SetRawInfo) {
  CompanyInfo company;
  company.SetRawInfo(COMPANY_NAME, u"Google");
  EXPECT_EQ(company.GetRawInfo(COMPANY_NAME), u"Google");
}

TEST(CompanyTest, IsValid) {
  auto set_and_validate = [](const std::u16string& company_name) {
    CompanyInfo company;
    company.SetRawInfo(COMPANY_NAME, company_name);
    return company.IsValid();
  };

  EXPECT_TRUE(set_and_validate(u"Google"));
  EXPECT_TRUE(set_and_validate(u"1818"));
  EXPECT_FALSE(set_and_validate(u"1987"));
  EXPECT_FALSE(set_and_validate(u"2019"));
  EXPECT_TRUE(set_and_validate(u"2345"));
  EXPECT_TRUE(set_and_validate(u"It was 1987."));
  EXPECT_TRUE(set_and_validate(u"1987 was the year."));
  EXPECT_FALSE(set_and_validate(u"Mr"));
  EXPECT_FALSE(set_and_validate(u"Mr."));
  EXPECT_FALSE(set_and_validate(u"Mrs"));
  EXPECT_FALSE(set_and_validate(u"Mrs."));
  EXPECT_TRUE(set_and_validate(u"Mr. & Mrs."));
  EXPECT_TRUE(set_and_validate(u"Mr. & Mrs. Smith"));
  EXPECT_FALSE(set_and_validate(u"Frau"));
  EXPECT_TRUE(set_and_validate(u"Frau Doktor"));
  EXPECT_FALSE(set_and_validate(u"Herr"));
  EXPECT_FALSE(set_and_validate(u"Mme"));
  EXPECT_FALSE(set_and_validate(u"Ms"));
  EXPECT_FALSE(set_and_validate(u"Dr"));
  EXPECT_FALSE(set_and_validate(u"Dr."));
  EXPECT_FALSE(set_and_validate(u"Prof"));
  EXPECT_FALSE(set_and_validate(u"Prof."));
}

}  // namespace autofill
