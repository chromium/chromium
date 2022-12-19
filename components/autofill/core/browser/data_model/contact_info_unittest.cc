// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/contact_info.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
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

class SetFullNameTest : public testing::TestWithParam<FullNameTestCase> {};

TEST_P(SetFullNameTest, SetFullName) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.full_name_input);

  NameInfo name;
  name.SetInfo(AutofillType(NAME_FULL), ASCIIToUTF16(test_case.full_name_input),
               "en-US");
  name.FinalizeAfterImport();
  EXPECT_EQ(ASCIIToUTF16(test_case.given_name_output),
            name.GetInfo(AutofillType(NAME_FIRST), "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.middle_name_output),
            name.GetInfo(AutofillType(NAME_MIDDLE), "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.family_name_output),
            name.GetInfo(AutofillType(NAME_LAST), "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.full_name_input),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));
}

TEST(NameInfoTest, GetMatchingTypesWithPrefix) {
  base::test::ScopedFeatureList structured_name_feature;
  structured_name_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForHonorificPrefixes);

  NameInfo name;
  test::FormGroupValues name_values = {
      {.type = NAME_FULL_WITH_HONORIFIC_PREFIX,
       .value = "Mr. Pablo Diego Ruiz y Picasso",
       .verification_status = VerificationStatus::kObserved}};
  test::SetFormGroupValues(name, name_values);
  name.FinalizeAfterImport();

  test::FormGroupValues expectation = {
      {.type = NAME_FULL_WITH_HONORIFIC_PREFIX,
       .value = "Mr. Pablo Diego Ruiz y Picasso",
       .verification_status = VerificationStatus::kObserved},
      {.type = NAME_HONORIFIC_PREFIX,
       .value = "Mr.",
       .verification_status = VerificationStatus::kParsed},
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

  ServerFieldTypeSet matching_types;
  name.GetMatchingTypes(u"Ruiz", "US", &matching_types);
  EXPECT_EQ(matching_types, ServerFieldTypeSet({NAME_LAST_FIRST}));

  name.GetMatchingTypes(u"Mr.", "US", &matching_types);
  EXPECT_EQ(matching_types,
            ServerFieldTypeSet({NAME_LAST_FIRST, NAME_HONORIFIC_PREFIX}));

  // Verify that a field filled with |NAME_FULL_WITH_HONORIFIC_PREFIX| creates a
  // |NAME_FULL| vote.
  name.GetMatchingTypes(u"Mr. Pablo Diego Ruiz y Picasso", "US",
                        &matching_types);
  EXPECT_EQ(matching_types, ServerFieldTypeSet({NAME_FULL, NAME_LAST_FIRST,
                                                NAME_HONORIFIC_PREFIX}));
}

TEST(NameInfoTest, GetMatchingTypes) {
  base::test::ScopedFeatureList structured_name_feature;
  structured_name_feature.InitAndDisableFeature(
      features::kAutofillEnableSupportForHonorificPrefixes);

  NameInfo name;

  test::FormGroupValues name_values = {
      {.type = NAME_FULL,
       .value = "Mr. Pablo Diego Ruiz y Picasso",
       .verification_status = VerificationStatus::kObserved}};
  test::SetFormGroupValues(name, name_values);
  name.FinalizeAfterImport();

  test::FormGroupValues expectation = {
      {.type = NAME_HONORIFIC_PREFIX,
       .value = "",
       .verification_status = VerificationStatus::kNoStatus},
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

  ServerFieldTypeSet matching_types;
  name.GetMatchingTypes(u"Ruiz", "US", &matching_types);
  EXPECT_EQ(matching_types, ServerFieldTypeSet({NAME_LAST_FIRST}));

  // The honorific prefix is ignored.
  name.GetMatchingTypes(u"Mr.", "US", &matching_types);
  EXPECT_EQ(matching_types, ServerFieldTypeSet({NAME_LAST_FIRST}));
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
                         "Ignatius Conan", "Doyle"}));

TEST(CompanyTest, CompanyName) {
  AutofillProfile profile;
  CompanyInfo company(&profile);
  ASSERT_FALSE(profile.IsVerified());

  auto SetAndGetCompany = [&company](const char* company_name) mutable {
    company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16(company_name));
    return base::UTF16ToUTF8(company.GetRawInfo(COMPANY_NAME));
  };

  EXPECT_EQ(SetAndGetCompany("Google"), "Google");
  EXPECT_EQ(SetAndGetCompany("1818"), "1818");
  EXPECT_EQ(SetAndGetCompany("1987"), "");
  EXPECT_EQ(SetAndGetCompany("2019"), "");
  EXPECT_EQ(SetAndGetCompany("2345"), "2345");
  EXPECT_EQ(SetAndGetCompany("It was 1987."), "It was 1987.");
  EXPECT_EQ(SetAndGetCompany("1987 was the year."), "1987 was the year.");
  EXPECT_EQ(SetAndGetCompany("Mr"), "");
  EXPECT_EQ(SetAndGetCompany("Mr."), "");
  EXPECT_EQ(SetAndGetCompany("Mrs"), "");
  EXPECT_EQ(SetAndGetCompany("Mrs."), "");
  EXPECT_EQ(SetAndGetCompany("Mr. & Mrs."), "Mr. & Mrs.");
  EXPECT_EQ(SetAndGetCompany("Mr. & Mrs. Smith"), "Mr. & Mrs. Smith");
  EXPECT_EQ(SetAndGetCompany("Frau"), "");
  EXPECT_EQ(SetAndGetCompany("Frau Doktor"), "Frau Doktor");
  EXPECT_EQ(SetAndGetCompany("Herr"), "");
  EXPECT_EQ(SetAndGetCompany("Mme"), "");
  EXPECT_EQ(SetAndGetCompany("Ms"), "");
  EXPECT_EQ(SetAndGetCompany("Dr"), "");
  EXPECT_EQ(SetAndGetCompany("Dr."), "");
  EXPECT_EQ(SetAndGetCompany("Prof"), "");
  EXPECT_EQ(SetAndGetCompany("Prof."), "");

  profile.set_origin("Not empty");
  ASSERT_TRUE(profile.IsVerified());

  EXPECT_EQ(SetAndGetCompany("Google"), "Google");
  EXPECT_EQ(SetAndGetCompany("1818"), "1818");
  EXPECT_EQ(SetAndGetCompany("1987"), "1987");
  EXPECT_EQ(SetAndGetCompany("2019"), "2019");
  EXPECT_EQ(SetAndGetCompany("2345"), "2345");
  EXPECT_EQ(SetAndGetCompany("It was 1987."), "It was 1987.");
  EXPECT_EQ(SetAndGetCompany("1987 was the year."), "1987 was the year.");
  EXPECT_EQ(SetAndGetCompany("Mr"), "Mr");
  EXPECT_EQ(SetAndGetCompany("Mr."), "Mr.");
  EXPECT_EQ(SetAndGetCompany("Mrs"), "Mrs");
  EXPECT_EQ(SetAndGetCompany("Mrs."), "Mrs.");
  EXPECT_EQ(SetAndGetCompany("Mr. & Mrs."), "Mr. & Mrs.");
  EXPECT_EQ(SetAndGetCompany("Mr. & Mrs. Smith"), "Mr. & Mrs. Smith");
  EXPECT_EQ(SetAndGetCompany("Frau"), "Frau");
  EXPECT_EQ(SetAndGetCompany("Frau Doktor"), "Frau Doktor");
  EXPECT_EQ(SetAndGetCompany("Herr"), "Herr");
  EXPECT_EQ(SetAndGetCompany("Mme"), "Mme");
  EXPECT_EQ(SetAndGetCompany("Ms"), "Ms");
  EXPECT_EQ(SetAndGetCompany("Dr"), "Dr");
  EXPECT_EQ(SetAndGetCompany("Dr."), "Dr.");
  EXPECT_EQ(SetAndGetCompany("Prof"), "Prof");
  EXPECT_EQ(SetAndGetCompany("Prof."), "Prof.");
}

TEST(CompanyTest, CompanyNameSocialTitleCopy) {
  AutofillProfile profile;
  CompanyInfo company(&profile);
  ASSERT_FALSE(profile.IsVerified());


  CompanyInfo company_google(&profile);
  CompanyInfo company_year(&profile);
  CompanyInfo company_social_title(&profile);

  company_google.SetRawInfo(COMPANY_NAME, u"Google");
  company_year.SetRawInfo(COMPANY_NAME, u"1987");
  company_social_title.SetRawInfo(COMPANY_NAME, u"Dr");

  company_google = company_year;
  EXPECT_EQ(u"", company_google.GetRawInfo(COMPANY_NAME));
  company_google = company_social_title;
  EXPECT_EQ(u"", company_google.GetRawInfo(COMPANY_NAME));
}

TEST(CompanyTest, CompanyNameYearIsEqual) {
  AutofillProfile profile;
  ASSERT_FALSE(profile.IsVerified());

  CompanyInfo company_year(&profile);
  CompanyInfo company_social_title(&profile);

  company_year.SetRawInfo(COMPANY_NAME, u"1987");
  company_social_title.SetRawInfo(COMPANY_NAME, u"Dr");

  EXPECT_EQ(company_year, company_social_title);
}

}  // namespace autofill
