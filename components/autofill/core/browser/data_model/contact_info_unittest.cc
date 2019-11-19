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
  EXPECT_EQ(ASCIIToUTF16(test_case.given_name_output),
            name.GetInfo(AutofillType(NAME_FIRST), "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.middle_name_output),
            name.GetInfo(AutofillType(NAME_MIDDLE), "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.family_name_output),
            name.GetInfo(AutofillType(NAME_LAST), "en-US"));
  EXPECT_EQ(ASCIIToUTF16(test_case.full_name_input),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));
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
        FullNameTestCase{"John Smith, MA", "John", "", "Smith"},
        FullNameTestCase{"John Jacob Jingleheimer Smith", "John Jacob",
                         "Jingleheimer", "Smith"},
        FullNameTestCase{"Virgil", "Virgil", "", ""},
        FullNameTestCase{"Murray Gell-Mann", "Murray", "", "Gell-Mann"},
        FullNameTestCase{"Mikhail Yevgrafovich Saltykov-Shchedrin", "Mikhail",
                         "Yevgrafovich", "Saltykov-Shchedrin"},
        FullNameTestCase{"Arthur Ignatius Conan Doyle", "Arthur Ignatius",
                         "Conan", "Doyle"}));

TEST(NameInfoTest, GetFullName) {
  NameInfo name;
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Middle"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Middle"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Middle Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
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
  // inconsitent state.)
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

TEST(CompanyTest, CompanyNameYear) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillRejectCompanyBirthyear},
      /*disabled_features=*/{});

  AutofillProfile profile;
  CompanyInfo company(&profile);
  ASSERT_FALSE(profile.IsVerified());

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Google"));
  EXPECT_EQ(UTF8ToUTF16("Google"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("It was 1987."));
  EXPECT_EQ(UTF8ToUTF16("It was 1987."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987 was the year."));
  EXPECT_EQ(UTF8ToUTF16("1987 was the year."),
            company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Yes, 1987 was the year."));
  EXPECT_EQ(UTF8ToUTF16("Yes, 1987 was the year."),
            company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2019"));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1818"));
  EXPECT_EQ(UTF8ToUTF16("1818"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2345"));
  EXPECT_EQ(UTF8ToUTF16("2345"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr"));
  EXPECT_EQ(UTF8ToUTF16("Mr"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr."));
  EXPECT_EQ(UTF8ToUTF16("Mr."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs"));
  EXPECT_EQ(UTF8ToUTF16("Mrs"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs."));
  EXPECT_EQ(UTF8ToUTF16("Mrs."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs."));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs. Smith"));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs. Smith"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau"));
  EXPECT_EQ(UTF8ToUTF16("Frau"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau Doktor"));
  EXPECT_EQ(UTF8ToUTF16("Frau Doktor"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Herr"));
  EXPECT_EQ(UTF8ToUTF16("Herr"), company.GetRawInfo(COMPANY_NAME));

  profile.set_origin("Not empty");
  ASSERT_TRUE(profile.IsVerified());

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Google"));
  EXPECT_EQ(UTF8ToUTF16("Google"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));
  EXPECT_EQ(UTF8ToUTF16("1987"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2019"));
  EXPECT_EQ(UTF8ToUTF16("2019"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1818"));
  EXPECT_EQ(UTF8ToUTF16("1818"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2345"));
  EXPECT_EQ(UTF8ToUTF16("2345"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr"));
  EXPECT_EQ(UTF8ToUTF16("Mr"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr."));
  EXPECT_EQ(UTF8ToUTF16("Mr."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs"));
  EXPECT_EQ(UTF8ToUTF16("Mrs"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs."));
  EXPECT_EQ(UTF8ToUTF16("Mrs."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs."));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs. Smith"));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs. Smith"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau"));
  EXPECT_EQ(UTF8ToUTF16("Frau"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau Doktor"));
  EXPECT_EQ(UTF8ToUTF16("Frau Doktor"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Herr"));
  EXPECT_EQ(UTF8ToUTF16("Herr"), company.GetRawInfo(COMPANY_NAME));
}

TEST(CompanyTest, CompanyNameSocialTitle) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillRejectCompanySocialTitle},
      /*disabled_features=*/{});

  AutofillProfile profile;
  CompanyInfo company(&profile);
  ASSERT_FALSE(profile.IsVerified());

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Google"));
  EXPECT_EQ(UTF8ToUTF16("Google"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));
  EXPECT_EQ(UTF8ToUTF16("1987"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("It was 1987."));
  EXPECT_EQ(UTF8ToUTF16("It was 1987."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987 was the year."));
  EXPECT_EQ(UTF8ToUTF16("1987 was the year."),
            company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Yes, 1987 was the year."));
  EXPECT_EQ(UTF8ToUTF16("Yes, 1987 was the year."),
            company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2019"));
  EXPECT_EQ(UTF8ToUTF16("2019"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1818"));
  EXPECT_EQ(UTF8ToUTF16("1818"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2345"));
  EXPECT_EQ(UTF8ToUTF16("2345"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr"));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr."));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs"));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs."));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs."));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs. Smith"));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs. Smith"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau"));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau Doktor"));
  EXPECT_EQ(UTF8ToUTF16("Frau Doktor"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Herr"));
  EXPECT_EQ(UTF8ToUTF16(""), company.GetRawInfo(COMPANY_NAME));

  profile.set_origin("Not empty");
  ASSERT_TRUE(profile.IsVerified());

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Google"));
  EXPECT_EQ(UTF8ToUTF16("Google"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));
  EXPECT_EQ(UTF8ToUTF16("1987"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2019"));
  EXPECT_EQ(UTF8ToUTF16("2019"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1818"));
  EXPECT_EQ(UTF8ToUTF16("1818"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2345"));
  EXPECT_EQ(UTF8ToUTF16("2345"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr"));
  EXPECT_EQ(UTF8ToUTF16("Mr"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr."));
  EXPECT_EQ(UTF8ToUTF16("Mr."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs"));
  EXPECT_EQ(UTF8ToUTF16("Mrs"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mrs."));
  EXPECT_EQ(UTF8ToUTF16("Mrs."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs."));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs."), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Mr. & Mrs. Smith"));
  EXPECT_EQ(UTF8ToUTF16("Mr. & Mrs. Smith"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau"));
  EXPECT_EQ(UTF8ToUTF16("Frau"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Frau Doktor"));
  EXPECT_EQ(UTF8ToUTF16("Frau Doktor"), company.GetRawInfo(COMPANY_NAME));

  company.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Herr"));
  EXPECT_EQ(UTF8ToUTF16("Herr"), company.GetRawInfo(COMPANY_NAME));
}

TEST(CompanyTest, CompanyNameYearCopy) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillRejectCompanyBirthyear},
      /*disabled_features=*/{});

  AutofillProfile profile;
  ASSERT_FALSE(profile.IsVerified());

  CompanyInfo company_google(&profile);
  CompanyInfo company_year(&profile);

  company_google.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Google"));
  company_year.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));

  company_google = company_year;
  EXPECT_EQ(UTF8ToUTF16(""), company_google.GetRawInfo(COMPANY_NAME));
}

TEST(CompanyTest, CompanyNameYearIsEqual) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillRejectCompanyBirthyear},
      /*disabled_features=*/{});

  AutofillProfile profile;
  ASSERT_FALSE(profile.IsVerified());

  CompanyInfo company_old(&profile);
  CompanyInfo company_young(&profile);

  company_old.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("2019"));
  company_young.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("1987"));

  EXPECT_EQ(company_old, company_young);
}

TEST(CompanyTest, CompanyNameSocialTitleCopy) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillRejectCompanySocialTitle},
      /*disabled_features=*/{});

  AutofillProfile profile;
  ASSERT_FALSE(profile.IsVerified());

  CompanyInfo company_google(&profile);
  CompanyInfo company_year(&profile);

  company_google.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Google"));
  company_year.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Prof."));

  company_google = company_year;
  EXPECT_EQ(UTF8ToUTF16(""), company_google.GetRawInfo(COMPANY_NAME));
}

TEST(CompanyTest, CompanyNameSocialTitleIsEqual) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillRejectCompanySocialTitle},
      /*disabled_features=*/{});

  AutofillProfile profile;
  ASSERT_FALSE(profile.IsVerified());

  CompanyInfo company_old(&profile);
  CompanyInfo company_young(&profile);

  company_old.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Dr"));
  company_young.SetRawInfo(COMPANY_NAME, UTF8ToUTF16("Prof"));

  EXPECT_EQ(company_old, company_young);
}

}  // namespace autofill
