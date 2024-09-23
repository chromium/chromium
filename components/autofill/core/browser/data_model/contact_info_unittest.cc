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

TEST(NameInfoTest, GetMatchingTypes) {
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
  name.GetMatchingTypesWithProfileSources(u"Ruiz", "US", &matching_types,
                                          nullptr);
  EXPECT_EQ(matching_types, FieldTypeSet({NAME_LAST_FIRST}));

  // The honorific prefix is ignored.
  name.GetMatchingTypesWithProfileSources(u"Mr.", "US", &matching_types,
                                          nullptr);
  EXPECT_EQ(matching_types, FieldTypeSet({NAME_LAST_FIRST}));
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
