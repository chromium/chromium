// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/contact_info.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

using structured_address::VerificationStatus;

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

TEST(NameInfoTest, GetMatchingTypesForStructuredName) {
  base::test::ScopedFeatureList structured_name_feature;
  structured_name_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  NameInfo name;
  name.SetRawInfoWithVerificationStatus(
      NAME_FULL, base::ASCIIToUTF16("Mr. Pablo Diego Ruiz y Picasso"),
      VerificationStatus::kObserved);
  name.FinalizeAfterImport();

  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // EXPECT_EQ(name.GetRawInfo(NAME_HONORIFIC_PREFIX),
  // base::ASCIIToUTF16("Mr."));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), base::ASCIIToUTF16("Pablo Diego"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), base::ASCIIToUTF16(""));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), base::ASCIIToUTF16("Ruiz y Picasso"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST_FIRST), base::ASCIIToUTF16("Ruiz"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST_SECOND), base::ASCIIToUTF16("Picasso"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST_CONJUNCTION), base::ASCIIToUTF16("y"));

  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // EXPECT_EQ(name.GetVerificationStatus(NAME_HONORIFIC_PREFIX),
  //          VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatus(NAME_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatus(NAME_MIDDLE),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatus(NAME_LAST), VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatus(NAME_LAST_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatus(NAME_LAST_SECOND),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatus(NAME_LAST_CONJUNCTION),
            VerificationStatus::kParsed);

  ServerFieldTypeSet matching_types;
  name.GetMatchingTypes(base::ASCIIToUTF16("Ruiz"), "US", &matching_types);
  EXPECT_EQ(matching_types, ServerFieldTypeSet({NAME_LAST_FIRST}));

  name.GetMatchingTypes(base::ASCIIToUTF16("Mr."), "US", &matching_types);
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
        !structured_address::StructuredNamesEnabled()
            ? FullNameTestCase{"John Jacob Jingleheimer Smith", "John Jacob",
                               "Jingleheimer", "Smith"}
            : FullNameTestCase{"John Jacob Jingleheimer Smith", "John",
                               "Jacob Jingleheimer", "Smith"},
        !structured_address::StructuredNamesEnabled()
            ? FullNameTestCase{"Virgil", "Virgil", "", ""}
            : FullNameTestCase{"Virgil", "", "", "Virgil"},
        FullNameTestCase{"Murray Gell-Mann", "Murray", "", "Gell-Mann"},
        FullNameTestCase{"Mikhail Yevgrafovich Saltykov-Shchedrin", "Mikhail",
                         "Yevgrafovich", "Saltykov-Shchedrin"},
        !structured_address::StructuredNamesEnabled()
            ? FullNameTestCase{"Arthur Ignatius Conan Doyle", "Arthur Ignatius",
                               "Conan", "Doyle"}
            : FullNameTestCase{"Arthur Ignatius Conan Doyle", "Arthur",
                               "Ignatius Conan", "Doyle"}));

TEST(NameInfoTest, GetFullName) {
  // This test is not applicable to more-structured names because the logic of
  // handling the duality between first,middle,last and the full name works
  // differently.
  if (structured_address::StructuredNamesEnabled())
    return;

  NameInfo name;
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, base::string16());
  name.FinalizeAfterImport();
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  // For structured names, the full must contain all of its subcomponents.
  EXPECT_EQ(structured_address::StructuredNamesEnabled() ? ASCIIToUTF16("First")
                                                         : base::string16(),
            name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name = NameInfo();
  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  name.FinalizeAfterImport();
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Middle"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name = NameInfo();
  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  name.FinalizeAfterImport();
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name = NameInfo();
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  name.FinalizeAfterImport();
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Middle"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name = NameInfo();
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  name.FinalizeAfterImport();
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name = NameInfo();
  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  name.FinalizeAfterImport();
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Middle Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name = NameInfo();
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  name.FinalizeAfterImport();
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FULL, ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last, MD"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  // Setting a name to the value it already has: no change.
  name.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("First"), "en-US");
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last, MD"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  // Setting raw info: no change. (Even though this leads to a slightly
  // inconsistent state.)
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Second"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Second"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last, MD"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  // Changing something (e.g., the first name) clears the stored full name.
  name.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Third"), "en-US");
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Third"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(ASCIIToUTF16("Third Middle Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));
}

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

  company_google.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Google"));
  company_year.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));
  company_social_title.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Dr"));

  company_google = company_year;
  EXPECT_EQ(UTF8ToUTF16(""), company_google.GetRawInfo(COMPANY_NAME));
  company_google = company_social_title;
  EXPECT_EQ(UTF8ToUTF16(""), company_google.GetRawInfo(COMPANY_NAME));
}

TEST(CompanyTest, CompanyNameYearIsEqual) {
  AutofillProfile profile;
  ASSERT_FALSE(profile.IsVerified());

  CompanyInfo company_year(&profile);
  CompanyInfo company_social_title(&profile);

  company_year.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));
  company_social_title.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Dr"));

  EXPECT_EQ(company_year, company_social_title);
}

}  // namespace autofill
