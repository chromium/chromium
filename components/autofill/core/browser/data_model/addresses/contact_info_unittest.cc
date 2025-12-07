// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/contact_info.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {
namespace {

using i18n_model_definition::kLegacyHierarchyCountryCode;

constexpr char kLocale[] = "en-US";

}  // namespace

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

// Profiles are considered migrated if the regular name fields are empty and the
// alternative name fields are populated with regular name original values.
MATCHER_P(NameIsMigrated, original_profile, "") {
  return arg.record_type() == original_profile.record_type() &&
         original_profile.GetRawInfo(NAME_FULL) ==
             arg.GetRawInfo(ALTERNATIVE_FULL_NAME) &&
         original_profile.GetRawInfo(NAME_LAST) ==
             arg.GetRawInfo(ALTERNATIVE_FAMILY_NAME) &&
         original_profile.GetRawInfo(NAME_FIRST) ==
             arg.GetRawInfo(ALTERNATIVE_GIVEN_NAME) &&
         arg.GetRawInfo(NAME_FULL).empty() &&
         arg.GetNameInfo().GetRootForType(NAME_FULL)->AllDescendantsAreEmpty();
}

class SetFullNameTest : public testing::TestWithParam<FullNameTestCase> {};

TEST_P(SetFullNameTest, SetFullName) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.full_name_input);

  NameInfo name(/*alternative_names_supported=*/false);
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

  NameInfo name(/*alternative_names_supported=*/true);
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
 protected:
  void MergeNamesAndExpect(const NameInfo& a,
                           const NameInfo& b,
                           const NameInfo& expected) {
    NameInfo actual(/*alternative_names_supported=*/false);
    ASSERT_TRUE(NameInfo::MergeNames(a, kLegacyHierarchyCountryCode, b,
                                     kLegacyHierarchyCountryCode, actual));

    // Is the "processed" data correct?
    EXPECT_EQ(expected.GetInfo(NAME_FULL, kLocale),
              actual.GetInfo(NAME_FULL, kLocale));
    EXPECT_EQ(expected.GetInfo(NAME_FIRST, kLocale),
              actual.GetInfo(NAME_FIRST, kLocale));
    EXPECT_EQ(expected.GetInfo(NAME_MIDDLE, kLocale),
              actual.GetInfo(NAME_MIDDLE, kLocale));
    EXPECT_EQ(expected.GetInfo(NAME_LAST, kLocale),
              actual.GetInfo(NAME_LAST, kLocale));
    EXPECT_EQ(expected.GetInfo(ALTERNATIVE_FULL_NAME, kLocale),
              actual.GetInfo(ALTERNATIVE_FULL_NAME, kLocale));
    EXPECT_EQ(expected.GetInfo(ALTERNATIVE_GIVEN_NAME, kLocale),
              actual.GetInfo(ALTERNATIVE_GIVEN_NAME, kLocale));
    EXPECT_EQ(expected.GetInfo(ALTERNATIVE_FAMILY_NAME, kLocale),
              actual.GetInfo(ALTERNATIVE_FAMILY_NAME, kLocale));

    // Is the raw data correct?
    EXPECT_EQ(expected.GetRawInfo(NAME_FULL), actual.GetRawInfo(NAME_FULL));
    EXPECT_EQ(expected.GetRawInfo(NAME_FIRST), actual.GetRawInfo(NAME_FIRST));
    EXPECT_EQ(expected.GetRawInfo(NAME_MIDDLE), actual.GetRawInfo(NAME_MIDDLE));
    EXPECT_EQ(expected.GetRawInfo(NAME_LAST), actual.GetRawInfo(NAME_LAST));
    EXPECT_EQ(expected.GetRawInfo(ALTERNATIVE_FULL_NAME),
              actual.GetRawInfo(ALTERNATIVE_FULL_NAME));
    EXPECT_EQ(expected.GetRawInfo(ALTERNATIVE_GIVEN_NAME),
              actual.GetRawInfo(ALTERNATIVE_GIVEN_NAME));
    EXPECT_EQ(expected.GetRawInfo(ALTERNATIVE_FAMILY_NAME),
              actual.GetRawInfo(ALTERNATIVE_FAMILY_NAME));
  }

  NameInfo CreateNameInfo(const char16_t* first,
                          const char16_t* middle,
                          const char16_t* last,
                          const char16_t* full,
                          const char16_t* alternative_given = u"",
                          const char16_t* alternative_family = u"",
                          const char16_t* alternative_full = u"",
                          bool should_support_alternative_name = false) {
    NameInfo name(should_support_alternative_name);
    name.SetRawInfoWithVerificationStatus(NAME_FIRST, first,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(NAME_MIDDLE, middle,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(NAME_LAST, last,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(NAME_FULL, full,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME,
                                          alternative_given,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(ALTERNATIVE_FAMILY_NAME,
                                          alternative_family,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(
        ALTERNATIVE_FULL_NAME, alternative_full, VerificationStatus::kObserved);
    name.FinalizeAfterImport();
    return name;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillSupportPhoneticNameForJP};
};

TEST_F(NameInfoTest, GetMatchingTypes) {
  NameInfo name(/*alternative_names_supported=*/false);

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
  NameInfo name(/*alternative_names_supported=*/true);

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
  NameInfo name(/*alternative_names_supported=*/true);

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
  NameInfo name(/*alternative_names_supported=*/true);

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
  NameInfo name(/*alternative_names_supported=*/true);

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
  NameInfo name1(/*alternative_names_supported=*/false);
  test::SetFormGroupValues(name1, {{.type = NAME_FULL, .value = "John Doe"}});

  NameInfo name2(/*alternative_names_supported=*/false);
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
  NameInfo stored_profile(
      /*alternative_names_supported=*/true);
  test::SetFormGroupValues(
      stored_profile,
      {{.type = ALTERNATIVE_FAMILY_NAME, .value = "やまもと"},
       {.type = ALTERNATIVE_FULL_NAME, .value = "やまもと あおい"}});

  NameInfo submitted_data(
      /*alternative_names_supported=*/true);
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
  NameInfo stored_profile(
      /*alternative_names_supported=*/true);
  test::SetFormGroupValues(
      stored_profile, {{.type = NAME_FULL, .value = "John Doe"},
                       {.type = ALTERNATIVE_FULL_NAME, .value = "John Doe"}});

  NameInfo submitted_data(
      /*alternative_names_supported=*/true);
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

TEST_F(NameInfoTest, MergeNames_WithPermutation) {
  // The first name has an observed structure.
  NameInfo name1(/*alternative_names_supported=*/false);
  name1.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Thomas",
                                         VerificationStatus::kObserved);
  name1.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"A.",
                                         VerificationStatus::kObserved);
  name1.SetRawInfoWithVerificationStatus(NAME_LAST, u"Anderson",
                                         VerificationStatus::kObserved);
  name1.FinalizeAfterImport();

  EXPECT_EQ(name1.GetRawInfo(NAME_FULL), u"Thomas A. Anderson");
  EXPECT_EQ(name1.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kFormatted);

  // The second name has an observed full name that uses a custom formatting.
  NameInfo name2(/*alternative_names_supported=*/false);
  name2.SetRawInfoWithVerificationStatus(NAME_FULL, u"Anderson, Thomas A.",
                                         VerificationStatus::kObserved);
  name2.FinalizeAfterImport();

  NameInfo merged_name(/*alternative_names_supported=*/false);
  NameInfo::MergeNames(name1, kLegacyHierarchyCountryCode, name2,
                       kLegacyHierarchyCountryCode, merged_name);

  // The merged name should maintain the structure but use the observation of
  // the custom-formatted full name.
  EXPECT_EQ(merged_name.GetRawInfo(NAME_FULL), u"Anderson, Thomas A.");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kObserved);
  EXPECT_EQ(merged_name.GetRawInfo(NAME_FIRST), u"Thomas");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(merged_name.GetRawInfo(NAME_MIDDLE), u"A.");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(merged_name.GetRawInfo(NAME_LAST), u"Anderson");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_LAST),
            VerificationStatus::kObserved);
}

TEST_F(NameInfoTest, MergeNames) {
  NameInfo name1(/*alternative_names_supported=*/false);
  name1.SetRawInfo(NAME_FULL, u"John Quincy Public");
  name1.SetRawInfo(NAME_FIRST, u"John");
  name1.SetRawInfo(NAME_MIDDLE, u"Quincy");
  name1.SetRawInfo(NAME_LAST, u"Public");
  name1.FinalizeAfterImport();

  NameInfo name2(/*alternative_names_supported=*/false);
  name2.SetRawInfo(NAME_FULL, u"John Q. Public");
  name2.SetRawInfo(NAME_FIRST, u"John");
  name2.SetRawInfo(NAME_MIDDLE, u"Q.");
  name2.SetRawInfo(NAME_LAST, u"Public");
  name2.FinalizeAfterImport();

  NameInfo name3(/*alternative_names_supported=*/false);
  name3.SetRawInfo(NAME_FULL, u"J Public");
  name3.SetRawInfo(NAME_FIRST, u"J");
  name3.SetRawInfo(NAME_MIDDLE, u"");
  name3.SetRawInfo(NAME_LAST, u"Public");
  name3.FinalizeAfterImport();

  NameInfo name4(/*alternative_names_supported=*/false);
  name4.SetRawInfo(NAME_FULL, u"John Quincy Public");
  name4.FinalizeAfterImport();

  NameInfo name5(/*alternative_names_supported=*/false);
  name5.SetRawInfo(NAME_FIRST, u"John");
  name5.SetRawInfo(NAME_LAST, u"Public");
  name5.FinalizeAfterImport();

  NameInfo synthesized(/*alternative_names_supported=*/false);
  synthesized.SetRawInfo(NAME_FULL, u"John Public");
  synthesized.SetRawInfo(NAME_FIRST, u"John");
  synthesized.SetRawInfo(NAME_MIDDLE, u"");
  synthesized.SetRawInfo(NAME_LAST, u"Public");
  synthesized.FinalizeAfterImport();

  MergeNamesAndExpect(name1, name1, name1);
  MergeNamesAndExpect(name1, name2, name1);
  MergeNamesAndExpect(name1, name3, name1);
  MergeNamesAndExpect(name1, name4, name1);
  MergeNamesAndExpect(name1, name5, name1);

  MergeNamesAndExpect(name2, name1, name1);
  MergeNamesAndExpect(name2, name2, name2);
  MergeNamesAndExpect(name2, name3, name2);
  MergeNamesAndExpect(name2, name4, name1);
  MergeNamesAndExpect(name2, name5, name2);

  MergeNamesAndExpect(name3, name1, name1);
  MergeNamesAndExpect(name3, name2, name2);
  MergeNamesAndExpect(name3, name3, name3);
  MergeNamesAndExpect(name3, name4, name1);
  MergeNamesAndExpect(name3, name5, synthesized);

  // name4 can be teased apart and reconstituted as name1.
  MergeNamesAndExpect(name4, name1, name1);
  MergeNamesAndExpect(name4, name2, name1);
  MergeNamesAndExpect(name4, name3, name1);
  MergeNamesAndExpect(name4, name4, name1);
  MergeNamesAndExpect(name4, name5, name1);

  // name5 expands the first name if it's not complete.
  MergeNamesAndExpect(name5, name1, name1);
  MergeNamesAndExpect(name5, name2, name2);
  MergeNamesAndExpect(name5, name3, synthesized);
  MergeNamesAndExpect(name5, name4, name1);
  MergeNamesAndExpect(name5, name5,
                      synthesized);  // We flesh out missing data.
}

// Regression test for crbug.com/324006880
TEST_F(NameInfoTest, MergeNamesWithWhitespaceDifferences) {
  NameInfo old_name(/*alternative_names_supported=*/false);
  old_name.SetRawInfo(NAME_FULL, u"Rafael de Paula");
  old_name.FinalizeAfterImport();

  NameInfo new_name(/*alternative_names_supported=*/false);
  new_name.SetRawInfo(NAME_FULL, u"Rafael dePaula");
  new_name.FinalizeAfterImport();

  auto info = CreateNameInfo(u"Rafael", u"", u"de Paula", u"Rafael de Paula");
  MergeNamesAndExpect(new_name, old_name, info);
}

TEST_F(NameInfoTest, MergeCJKNames) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillSupportPhoneticNameForJP};

  // Korean names that are all mergeable, but constructed differently.
  NameInfo name1 = CreateNameInfo(u"호", u"", u"이영", u"이영 호");
  NameInfo name2 = CreateNameInfo(u"이영호", u"", u"", u"이영호");
  NameInfo name3 = CreateNameInfo(u"영호", u"", u"이", u"이영호");
  NameInfo name4 = CreateNameInfo(u"영호", u"", u"이", u"");
  NameInfo name5 = CreateNameInfo(u"영호", u"", u"이", u"이 영호");

  // Mergeable foreign name in Japanese with a 'KATAKANA MIDDLE DOT'.
  NameInfo name6 = CreateNameInfo(u"", u"", u"", u"ゲイツ・ビル");
  NameInfo name7 = CreateNameInfo(u"ビル", u"", u"ゲイツ", u"");

  // Mergeable foreign name in Japanese with a 'KATAKANA MIDDLE DOT' and
  // phonetic name being present.
  NameInfo name8 =
      CreateNameInfo(u"", u"", u"", u"山本・葵", u"", u"", u"やまもと・あおい");
  NameInfo name9 =
      CreateNameInfo(u"葵", u"", u"山本", u"", u"あおい", u"やまもと", u"");

  // Mergeable foreign name in Japanese with a `　` and
  // phonetic name being present.
  NameInfo name10 =
      CreateNameInfo(u"", u"", u"", u"山本・葵", u"", u"", u"すずき　はるか");
  NameInfo name11 =
      CreateNameInfo(u"葵", u"", u"山本", u"", u"はるか", u"すずき", u"");

  // Because |name1| is the most recent, it always wins over others.
  MergeNamesAndExpect(name1, name2,
                      CreateNameInfo(u"호", u"", u"이영", u"이영 호"));
  MergeNamesAndExpect(name1, name3,
                      CreateNameInfo(u"호", u"", u"이영", u"이영 호"));
  MergeNamesAndExpect(name1, name4,
                      CreateNameInfo(u"호", u"", u"이영", u"이영 호"));
  MergeNamesAndExpect(name1, name5,
                      CreateNameInfo(u"호", u"", u"이영", u"이영 호"));

  // |name3| is more recent than |name4| and |name5|.
  MergeNamesAndExpect(name3, name4,
                      CreateNameInfo(u"영호", u"", u"이", u"이영호"));
  MergeNamesAndExpect(name3, name5,
                      CreateNameInfo(u"영호", u"", u"이", u"이영호"));

  // |name4| is more recent than |name5|. However, it does not have an explicit
  // full name, so use the one from |name5|.
  MergeNamesAndExpect(name4, name5,
                      CreateNameInfo(u"영호", u"", u"이", u"이 영호"));

  // There is no conflict between |name6| and |name7|, so use the parts from
  // both.
  MergeNamesAndExpect(name6, name7,
                      CreateNameInfo(u"ビル", u"", u"ゲイツ", u"ゲイツ・ビル"));
  // Japanese alternative names are mergeable.
  MergeNamesAndExpect(
      name8, name9,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと・あおい"));
  MergeNamesAndExpect(name10, name11,
                      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                     u"はるか", u"すずき", u"すずき　はるか"));
}

TEST_F(NameInfoTest, MergeCJKNamesWhereAlternativeNameNormalizationIsNeeded) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillSupportPhoneticNameForJP};

  // Phonetic name using Hiragana.
  NameInfo name1 = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい",
                                  u"やまもと", u"");
  // The same phonetic name, but saved as alternative_full_name with separator.
  NameInfo name2 = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい",
                                  u"やまもと", u"やまもと・あおい");
  // The same phonetic name, but saved as alternative_full_name with white space
  // separator.
  NameInfo name3 = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい",
                                  u"やまもと", u"やまもと あおい");

  // Semantically the same profiles as `name2`, `name3`, `name4`, but using
  // Katakana for alternative name.
  NameInfo name1_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"アオイ", u"ヤマモト", u"");
  NameInfo name2_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"", u"", u"ヤマモト・アオイ");
  NameInfo name3_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"", u"", u"ヤマモト アオイ");

  MergeNamesAndExpect(
      name2, name1_katakana,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと・あおい"));
  MergeNamesAndExpect(
      name3, name1_katakana,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと あおい"));
  MergeNamesAndExpect(
      name2, name2_katakana,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと・あおい"));
  MergeNamesAndExpect(
      name3, name2_katakana,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと あおい"));
  MergeNamesAndExpect(
      name2, name3_katakana,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと・あおい"));
  MergeNamesAndExpect(
      name3, name3_katakana,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと あおい"));

  // As long as the values in profile are semantically correct, it doesn't
  // matter what it the result of merging since db will do the transliteration
  // on save.
  MergeNamesAndExpect(
      name1, name1_katakana,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"アオイ", u"ヤマモト",
                     u"ヤマモト アオイ"));
  MergeNamesAndExpect(
      name1_katakana, name1,
      CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい", u"やまもと",
                     u"やまもと あおい"));
}

TEST_F(NameInfoTest, HaveMergeableAlternativeNames) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillSupportPhoneticNameForJP};

  NameInfo empty = CreateNameInfo(u"", u"", u"", u"", u"", u"", u"",
                                  /*should_support_alternative_name=*/true);

  // Latin characters only.
  NameInfo name1 = CreateNameInfo(u"John", u"", u"Smith", u"John Smith",
                                  u"Pjohn", u"Psmith", u"", true);
  NameInfo name1_mergeable = CreateNameInfo(
      u"John", u"", u"Smith", u"John Smith", u"", u"", u"Pjohn Psmith", true);

  // Phonetic name using Hiragana.
  NameInfo name2 = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"あおい",
                                  u"やまもと", u"", true);
  // The same phonetic name, but saved as alternative_full_name with separator.
  NameInfo name3 = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"", u"",
                                  u"やまもと・あおい", true);
  // The same phonetic name, but saved as alternative_full_name with white space
  // separator.
  NameInfo name4 = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵", u"", u"",
                                  u"やまもと あおい", true);

  // Semantically the same profiles as `name2`, `name3`, `name4`, but using
  // Katakana for alternative name.
  NameInfo name2_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"アオイ", u"ヤマモト", u"", true);
  NameInfo name3_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"", u"", u"ヤマモト・アオイ", true);
  NameInfo name4_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"", u"", u"ヤマモト アオイ", true);

  // Semantically the different profiles than `name2`, `name3`, `name4`, using
  // Katakana for alternative name.
  NameInfo name5_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"レイ", u"サクラ", u"", true);
  NameInfo name6_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"", u"", u"サクラ・レイ", true);
  NameInfo name7_katakana = CreateNameInfo(u"葵", u"", u"山本", u"山本・葵",
                                           u"", u"", u"サクラ レイ", true);

  // Base cases for latin characters.
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name1, AddressCountryCode("JP"), empty, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name1, AddressCountryCode("JP"), name1, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      empty, AddressCountryCode("JP"), name1, AddressCountryCode("JP")));

  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name1, AddressCountryCode("JP"), name1_mergeable,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name1_mergeable, AddressCountryCode("JP"), name1,
      AddressCountryCode("JP")));

  // CJK characters with empty profile.
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), empty, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3, AddressCountryCode("JP"), empty, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), empty,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3_katakana, AddressCountryCode("JP"), empty,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      empty, AddressCountryCode("JP"), name2, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      empty, AddressCountryCode("JP"), name3, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      empty, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      empty, AddressCountryCode("JP"), name3_katakana,
      AddressCountryCode("JP")));

  // Mergeable profiles using Hiragana.
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name2, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3, AddressCountryCode("JP"), name3, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4, AddressCountryCode("JP"), name4, AddressCountryCode("JP")));

  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name3, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3, AddressCountryCode("JP"), name2, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name4, AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4, AddressCountryCode("JP"), name2, AddressCountryCode("JP")));

  // Mergeable profiles using Katakana.
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name3_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4_katakana, AddressCountryCode("JP"), name4_katakana,
      AddressCountryCode("JP")));

  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name3_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3_katakana, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name4_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4_katakana, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));

  // Mergeable profiles where one is using Katakana and the other Hiragana.
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name3_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3_katakana, AddressCountryCode("JP"), name2,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name3,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));

  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name4_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4_katakana, AddressCountryCode("JP"), name2,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name4,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));

  // Semantically the same profiles one using Katakana the other Hiragana.
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name2,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3_katakana, AddressCountryCode("JP"), name3,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name3, AddressCountryCode("JP"), name3_katakana,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4_katakana, AddressCountryCode("JP"), name4,
      AddressCountryCode("JP")));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      name4, AddressCountryCode("JP"), name4_katakana,
      AddressCountryCode("JP")));

  // Non mergeable profiles where one is using Katakana and the other Hiragana.
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name6_katakana,
      AddressCountryCode("JP")));
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name6_katakana, AddressCountryCode("JP"), name2,
      AddressCountryCode("JP")));
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name2, AddressCountryCode("JP"), name7_katakana,
      AddressCountryCode("JP")));
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name7_katakana, AddressCountryCode("JP"), name2,
      AddressCountryCode("JP")));
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name7_katakana, AddressCountryCode("JP"), name4,
      AddressCountryCode("JP")));
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name4, AddressCountryCode("JP"), name7_katakana,
      AddressCountryCode("JP")));

  // Non mergeable profiles where both are using Katakana.
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name2_katakana, AddressCountryCode("JP"), name5_katakana,
      AddressCountryCode("JP")));
  EXPECT_FALSE(NameInfo::AreAlternativeNamesMergeable(
      name5_katakana, AddressCountryCode("JP"), name2_katakana,
      AddressCountryCode("JP")));
}

TEST_F(NameInfoTest, IsNameVariantOf) {
  const std::u16string kNormalizedFullName = u"timothe noel etienne perier";
  NameInfo name_info =
      CreateNameInfo(u"", u"", u"", u"Timothe Noël Etienne Perier");

  EXPECT_TRUE(name_info.IsNameVariantOf(kNormalizedFullName, kLocale));
  EXPECT_TRUE(name_info.IsNameVariantOf(u"t noel etienne perier", kLocale));
  EXPECT_TRUE(name_info.IsNameVariantOf(u"timothe perier", kLocale));
  EXPECT_TRUE(name_info.IsNameVariantOf(u"t perier", kLocale));
  EXPECT_TRUE(name_info.IsNameVariantOf(u"noel perier", kLocale));
  EXPECT_TRUE(name_info.IsNameVariantOf(u"t n etienne perier", kLocale));
  EXPECT_TRUE(name_info.IsNameVariantOf(u"tn perier", kLocale));
  EXPECT_TRUE(name_info.IsNameVariantOf(u"te perier", kLocale));

  EXPECT_FALSE(name_info.IsNameVariantOf(u"etienne noel perier", kLocale));
}

TEST_F(NameInfoTest, HaveMergeableNames) {
  NameInfo empty = CreateNameInfo(u"", u"", u"", u"");

  NameInfo p1 = CreateNameInfo(u"sven-åke", u"", u"larsson", u"");
  NameInfo p2 = CreateNameInfo(u"Åke", u"", u"Larsson", u"");
  NameInfo p3 = CreateNameInfo(u"A", u"", u"Larsson", u"");
  NameInfo p4 = CreateNameInfo(u"sven", u"ake", u"Larsson", u"");

  NameInfo initials = CreateNameInfo(u"SA", u"", u"Larsson", u"");

  NameInfo different = CreateNameInfo(u"Joe", u"", u"Larsson", u"");

  // |p1|, |p2|, |p3|, |p4| and |empty| should all be the mergeable with
  // one another. The order of the comparands should not matter.
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p1, kLegacyHierarchyCountryCode,
                                          empty, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p1, kLegacyHierarchyCountryCode, p1,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p1, kLegacyHierarchyCountryCode, p2,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p1, kLegacyHierarchyCountryCode, p3,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p1, kLegacyHierarchyCountryCode, p4,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p2, kLegacyHierarchyCountryCode,
                                          empty, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p2, kLegacyHierarchyCountryCode, p1,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p2, kLegacyHierarchyCountryCode, p2,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p2, kLegacyHierarchyCountryCode, p3,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p2, kLegacyHierarchyCountryCode, p4,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p3, kLegacyHierarchyCountryCode,
                                          empty, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p3, kLegacyHierarchyCountryCode, p1,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p3, kLegacyHierarchyCountryCode, p2,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p3, kLegacyHierarchyCountryCode, p3,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p3, kLegacyHierarchyCountryCode, p4,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p4, kLegacyHierarchyCountryCode,
                                          empty, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p4, kLegacyHierarchyCountryCode, p1,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p4, kLegacyHierarchyCountryCode, p2,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p4, kLegacyHierarchyCountryCode, p3,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(p4, kLegacyHierarchyCountryCode, p4,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(empty, kLegacyHierarchyCountryCode,
                                          empty, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(empty, kLegacyHierarchyCountryCode,
                                          p1, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(empty, kLegacyHierarchyCountryCode,
                                          p2, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(empty, kLegacyHierarchyCountryCode,
                                          p3, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(empty, kLegacyHierarchyCountryCode,
                                          p4, kLegacyHierarchyCountryCode));

  // |initials| is mergeable with |p1| and |p4| but not |p2| or |p3|.
  EXPECT_TRUE(NameInfo::AreNamesMergeable(initials, kLegacyHierarchyCountryCode,
                                          empty, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(initials, kLegacyHierarchyCountryCode,
                                          p1, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(initials, kLegacyHierarchyCountryCode,
                                          p4, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(empty, kLegacyHierarchyCountryCode,
                                          initials,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(
      p1, kLegacyHierarchyCountryCode, initials, kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreNamesMergeable(
      p4, kLegacyHierarchyCountryCode, initials, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      initials, kLegacyHierarchyCountryCode, p2, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      initials, kLegacyHierarchyCountryCode, p3, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      p2, kLegacyHierarchyCountryCode, initials, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      p3, kLegacyHierarchyCountryCode, initials, kLegacyHierarchyCountryCode));

  // None of the non-empty profiles should match |different|. The order of the
  // comparands should not matter.
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      p1, kLegacyHierarchyCountryCode, different, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      p2, kLegacyHierarchyCountryCode, different, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      p3, kLegacyHierarchyCountryCode, different, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      p4, kLegacyHierarchyCountryCode, different, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(
      NameInfo::AreNamesMergeable(initials, kLegacyHierarchyCountryCode,
                                  different, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      different, kLegacyHierarchyCountryCode, p1, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      different, kLegacyHierarchyCountryCode, p2, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      different, kLegacyHierarchyCountryCode, p3, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(
      different, kLegacyHierarchyCountryCode, p4, kLegacyHierarchyCountryCode));
  EXPECT_FALSE(
      NameInfo::AreNamesMergeable(different, kLegacyHierarchyCountryCode,
                                  initials, kLegacyHierarchyCountryCode));
}

TEST_F(NameInfoTest, HaveMergeableNamesWithGermanTransliteration) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableGermanTransliteration};

  NameInfo p1_de = CreateNameInfo(u"Hänsel", u"", u"Köhn", u"", u"", u"");
  NameInfo p2_de = CreateNameInfo(u"Haensel", u"", u"Koehn", u"");
  NameInfo p1_us = CreateNameInfo(u"Hänsel", u"", u"Köhn", u"", u"", u"");
  NameInfo p2_us = CreateNameInfo(u"Haensel", u"", u"Koehn", u"");

  EXPECT_TRUE(NameInfo::AreNamesMergeable(p1_de, AddressCountryCode("DE"),
                                          p2_de, AddressCountryCode("AT")));
  EXPECT_FALSE(NameInfo::AreNamesMergeable(p1_us, AddressCountryCode("US"),
                                           p2_us, AddressCountryCode("US")));
}

// Tests that name info with additional last name is mergeable with the
// existing one and merging works correctly.
TEST_F(NameInfoTest, NameInfoWithAdditionalLastNameIsMergeable) {
  NameInfo ni1 = CreateNameInfo(u"John", u"", u"", u"", u"", u"");
  NameInfo ni2 = CreateNameInfo(u"John", u"", u"Doe", u"", u"", u"");
  EXPECT_TRUE(NameInfo::AreNamesMergeable(ni1, kLegacyHierarchyCountryCode, ni2,
                                          kLegacyHierarchyCountryCode));

  NameInfo expected = CreateNameInfo(u"John", u"", u"Doe", u"", u"", u"");
  NameInfo actual(/*alternative_names_supported=*/false);
  NameInfo::MergeNames(ni1, kLegacyHierarchyCountryCode, ni2,
                       kLegacyHierarchyCountryCode, actual);
  EXPECT_EQ(expected, actual);
}

// Tests that name info with an additional middle name is mergeable with
// existing one.
TEST_F(NameInfoTest, NameInfoWithExtraMiddleNameIsMergeable) {
  NameInfo ni1 = CreateNameInfo(u"John", u"", u"Kennedy", u"", u"", u"");
  NameInfo ni2 = CreateNameInfo(u"John", u"Fitzgerald", u"Kennedy", u"", u"", u"");
  EXPECT_TRUE(NameInfo::AreNamesMergeable(ni1, kLegacyHierarchyCountryCode,
                                          ni2, kLegacyHierarchyCountryCode));

  NameInfo expected = CreateNameInfo(u"John", u"Fitzgerald", u"Kennedy", u"", u"", u"");
  NameInfo actual(/*alternative_names_supported=*/false);
  NameInfo::MergeNames(ni1, kLegacyHierarchyCountryCode, ni2,
                           kLegacyHierarchyCountryCode, actual);
  EXPECT_EQ(expected, actual);
}

TEST_F(NameInfoTest, SettingAndGettingNotSupportedAlternativeNames) {
  NameInfo ni1 = CreateNameInfo(u"John", u"", u"Doe", u"", u"alt_given",
                                u"alt_family", u"alt_familyalt_given",
                                /*should_support_alternative_name=*/false);
  ni1.FinalizeAfterImport();
  EXPECT_THAT(ni1.GetRawInfo(ALTERNATIVE_GIVEN_NAME), testing::IsEmpty());
  EXPECT_THAT(ni1.GetRawInfo(ALTERNATIVE_FAMILY_NAME), testing::IsEmpty());
  EXPECT_THAT(ni1.GetRawInfo(ALTERNATIVE_FULL_NAME), testing::IsEmpty());
}

TEST_F(NameInfoTest,
       SettingAndGettingVerificationStatusOfNotSupportedAlternativeNames) {
  NameInfo ni1 = CreateNameInfo(u"John", u"", u"Doe", u"", u"alt_given",
                                u"alt_family", u"alt_familyalt_given",
                                /*should_support_alternative_name=*/false);
  ni1.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"Testing",
                                       VerificationStatus::kObserved);
  ni1.SetRawInfoWithVerificationStatus(ALTERNATIVE_FAMILY_NAME, u"Tester",
                                       VerificationStatus::kObserved);
  ni1.SetRawInfoWithVerificationStatus(ALTERNATIVE_FULL_NAME, u"TestingTester",
                                       VerificationStatus::kObserved);
  EXPECT_EQ(ni1.GetVerificationStatus(ALTERNATIVE_GIVEN_NAME),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(ni1.GetVerificationStatus(ALTERNATIVE_FAMILY_NAME),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(ni1.GetVerificationStatus(ALTERNATIVE_FULL_NAME),
            VerificationStatus::kNoStatus);
}

TEST_F(NameInfoTest, MergingNotSupportedAlternativeNames) {
  NameInfo ni1 = CreateNameInfo(u"John", u"", u"Doe", u"", u"", u"", u"",
                                /*should_support_alternative_name=*/false);
  NameInfo ni2 = CreateNameInfo(u"John", u"H.", u"Doe", u"", u"", u"", u"",
                                /*should_support_alternative_name=*/false);

  EXPECT_TRUE(NameInfo::AreNamesMergeable(ni1, kLegacyHierarchyCountryCode, ni2,
                                          kLegacyHierarchyCountryCode));
  EXPECT_TRUE(NameInfo::AreAlternativeNamesMergeable(
      ni1, kLegacyHierarchyCountryCode, ni2, kLegacyHierarchyCountryCode));

  NameInfo result(/*alternative_names_supported=*/false);
  NameInfo::MergeNames(ni1, kLegacyHierarchyCountryCode, ni2,
                       kLegacyHierarchyCountryCode, result);
  EXPECT_FALSE(result.GetRawInfo(NAME_FULL).empty());
  EXPECT_THAT(result.GetRawInfo(ALTERNATIVE_GIVEN_NAME), testing::IsEmpty());
  EXPECT_THAT(result.GetRawInfo(ALTERNATIVE_FAMILY_NAME), testing::IsEmpty());
  EXPECT_THAT(result.GetRawInfo(ALTERNATIVE_FULL_NAME), testing::IsEmpty());
}

TEST_F(NameInfoTest, AssigningNameInfoWithAlternativeName) {
  NameInfo us_profile =
      CreateNameInfo(u"John", u"H.", u"Doe", u"", u"", u"", u"",
                     /*should_support_alternative_name=*/false);
  NameInfo jp_profile =
      CreateNameInfo(u"John", u"", u"Doe", u"", u"alt_given", u"alt_family",
                     u"alt_familyalt_given",
                     /*should_support_alternative_name=*/true);

  // The alternative names should be copied to the new profile.
  NameInfo new_profile = jp_profile;
  EXPECT_EQ(new_profile.GetRawInfo(ALTERNATIVE_GIVEN_NAME), u"alt_given");
  EXPECT_EQ(new_profile.GetRawInfo(ALTERNATIVE_FAMILY_NAME), u"alt_family");
  EXPECT_EQ(new_profile.GetRawInfo(ALTERNATIVE_FULL_NAME),
            u"alt_familyalt_given");

  // The US profile doesn't support alternative names so they should be cleared.
  new_profile = us_profile;
  EXPECT_THAT(new_profile.GetRawInfo(ALTERNATIVE_GIVEN_NAME),
              testing::IsEmpty());
  EXPECT_THAT(new_profile.GetRawInfo(ALTERNATIVE_FAMILY_NAME),
              testing::IsEmpty());
  EXPECT_THAT(new_profile.GetRawInfo(ALTERNATIVE_FULL_NAME),
              testing::IsEmpty());

  // Assigning a profile with alternative names should make them re-appear.
  new_profile = jp_profile;
  EXPECT_EQ(new_profile.GetRawInfo(ALTERNATIVE_GIVEN_NAME), u"alt_given");
  EXPECT_EQ(new_profile.GetRawInfo(ALTERNATIVE_FAMILY_NAME), u"alt_family");
  EXPECT_EQ(new_profile.GetRawInfo(ALTERNATIVE_FULL_NAME),
            u"alt_familyalt_given");
}

struct GetStorableTypeOfTestCase {
  FieldType input;
  std::optional<FieldType> expected;
};

class NameInfoGetStorableTypeOfTest
    : public NameInfoTest,
      public testing::WithParamInterface<GetStorableTypeOfTestCase> {};

TEST_P(NameInfoGetStorableTypeOfTest, GetStorableTypeOf) {
  NameInfo name_info = CreateNameInfo(u"John", u"", u"Doe", u"", u"", u"", u"",
                                      /*should_support_alternative_name=*/true);
  EXPECT_EQ(name_info.GetStorableTypeOf(GetParam().input),
            GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NameInfoGetStorableTypeOfTest,
    testing::Values(
        GetStorableTypeOfTestCase{NAME_MIDDLE_INITIAL,
                                  std::make_optional(NAME_MIDDLE)},
        GetStorableTypeOfTestCase{NAME_FULL, std::make_optional(NAME_FULL)},
        GetStorableTypeOfTestCase{ALTERNATIVE_GIVEN_NAME,
                                  std::make_optional(ALTERNATIVE_GIVEN_NAME)}));

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

struct NameMigrationTestCase {
  std::string name;
  bool should_migrate;
  AddressCountryCode country_code = AddressCountryCode("JP");
};

class NameInfoNameMigrationTest
    : public NameInfoTest,
      public testing::WithParamInterface<NameMigrationTestCase> {};

// Profiles that contain phonetic names stored in regular name fields should,
// migrate them to the alternative name fields. Those that do have other
// characters than phonetic symbols should not be migrated.
TEST_P(NameInfoNameMigrationTest, NameMigration) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  AutofillProfile profile(GetParam().country_code);

  profile.SetRawInfo(NAME_FULL, base::UTF8ToUTF16(GetParam().name));
  profile.FinalizeAfterImport();
  EXPECT_EQ(profile.GetNameInfo().HasNameEligibleForPhoneticNameMigration(),
            GetParam().should_migrate);

  if (GetParam().should_migrate) {
    AutofillProfile migrated_profile = profile;
    migrated_profile.MigrateRegularNameToPhoneticName();
    EXPECT_THAT(migrated_profile, NameIsMigrated(profile));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NameInfoNameMigrationTest,
    testing::Values(
        // Only katakana.
        NameMigrationTestCase{"メタワ", true},
        NameMigrationTestCase{"ワ 　タシ", true},
        NameMigrationTestCase{"メタ-ワ", true},
        NameMigrationTestCase{"メタ・ ワ", true},
        // Only hiragana.
        NameMigrationTestCase{"ねこです", true},
        NameMigrationTestCase{"ねこ です", true},
        NameMigrationTestCase{"ねこ-です", true},
        NameMigrationTestCase{"ねこ・です", true},
        NameMigrationTestCase{"ねこ・ 　です", true},
        // The following names should not be migrated
        // because they contain Latin characters.
        NameMigrationTestCase{"John Doe", false},
        NameMigrationTestCase{"John-Doe", false},
        NameMigrationTestCase{"John・Doe", false},
        NameMigrationTestCase{"abcメタワ", false},
        NameMigrationTestCase{"abcワ 　タシ", false},
        NameMigrationTestCase{"abc-ワ", false},
        NameMigrationTestCase{"メタ・ ワab", false},
        NameMigrationTestCase{"abcねこです", false},
        NameMigrationTestCase{"abcねこ です", false},
        NameMigrationTestCase{"ねこ-ですabc", false},
        NameMigrationTestCase{"abcねこ・です", false},
        NameMigrationTestCase{"ねこ・ 　ですabc", false},
        // The following names should not be migrated
        // because they contain Kanji characters.
        NameMigrationTestCase{"静夢メタワ", false},
        NameMigrationTestCase{"静夢ワ 　タシ", false},
        NameMigrationTestCase{"a静夢-ワ", false},
        NameMigrationTestCase{"メタ・ ワ静夢", false},
        NameMigrationTestCase{"a静ねこです", false},
        // Non Japanese profiles shouldn't offer migration.
        NameMigrationTestCase{"メタワ", false, AddressCountryCode("US")},
        NameMigrationTestCase{"ねこです", false, AddressCountryCode("US")}));

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
