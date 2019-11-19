// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/label_formatter_utils.h"

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

TEST(LabelFormatterUtilsTest, HaveSameFirstNames_OneProfileAndNoFirstName) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "", "", "DE",
                       "");
  EXPECT_TRUE(HaveSameFirstNames({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSameFirstNames_OneProfileAndFirstName) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "", "", "", "", "", "", "", "", "",
                       "DE", "");
  EXPECT_TRUE(HaveSameFirstNames({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSameFirstNames_NoFirstNames) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "", "", "Kirch", "", "", "", "", "", "", "",
                       "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "", "Winckelmann", "", "", "", "", "", "",
                       "", "DE", "");
  EXPECT_TRUE(HaveSameFirstNames({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSameFirstNames_SameFirstNames) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "", "Kirch", "", "", "", "", "", "",
                       "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "", "Winckelmann", "", "", "", "",
                       "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameFirstNames({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSameFirstNames_DifferentNonEmptyFirstNames) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "", "Kirch", "", "", "", "", "", "",
                       "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Mary", "", "Kirch", "", "", "", "", "", "",
                       "", "DE", "");
  EXPECT_FALSE(HaveSameFirstNames({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSameFirstNames_NonEmptyAndEmptyFirstNames) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "Margaretha", "Winckelmann", "", "", "",
                       "", "", "", "", "DE", "");
  EXPECT_FALSE(HaveSameFirstNames({&profile1, &profile2}, "de"));
  EXPECT_FALSE(HaveSameFirstNames({&profile2, &profile1}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddresses_OneProfileAndNoEmailAddress) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddresses_OneProfileAndEmailAddress) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch", "", "", "", "",
                       "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSameEmailAddresses_NoEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSameEmailAddresses_SameEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddresses_DifferentNonEmptyEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann",
                       "mmw@gmail.com", "", "", "", "", "", "", "DE", "");
  EXPECT_FALSE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddresses_NonEmptyAndEmptyEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_FALSE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
  EXPECT_FALSE(HaveSameEmailAddresses({&profile2, &profile1}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSamePhoneNumbers_OneProfileAndNoPhoneNumber) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch", "", "", "", "",
                       "", "", "", "DE", "");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSamePhoneNumbers_OneProfileAndPhoneNumber) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch", "", "", "", "",
                       "", "", "", "DE", "+49 30 4504-2823");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSamePhoneNumbers_NoPhoneNumber) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest, HaveSamePhoneNumbers_SamePhoneNumbers) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "+49 30 4504-2823");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "493045042823");
  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "03045042823");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile1, &profile2, &profile3}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbers_DifferentNonEmptyPhoneNumbers) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "+49 30 4504-2823");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "+49 221 22123828");
  EXPECT_FALSE(HaveSamePhoneNumbers({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbers_NonEmptyAndEmptyPhoneNumbers) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "+49 30 4504-2823");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_FALSE(HaveSamePhoneNumbers({&profile1, &profile2}, "de"));
  EXPECT_FALSE(HaveSamePhoneNumbers({&profile2, &profile1}, "de"));
}

TEST(LabelFormatterUtilsTest, GetLabelName) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  profile.SetInfo(NAME_FULL, base::ASCIIToUTF16("Maria Margaretha Kirch"),
                  "de");

  EXPECT_EQ(base::ASCIIToUTF16("Maria Margaretha Kirch"),
            GetLabelName({NAME_SUFFIX, NAME_FULL}, profile, "de"));
  EXPECT_EQ(base::ASCIIToUTF16("Maria Kirch"),
            GetLabelName({NAME_SUFFIX, NAME_FIRST, NAME_LAST}, profile, "de"));
  EXPECT_EQ(base::ASCIIToUTF16("Maria"),
            GetLabelName({NAME_SUFFIX, NAME_FIRST}, profile, "de"));
  EXPECT_EQ(base::ASCIIToUTF16("Kirch"),
            GetLabelName({NAME_SUFFIX, NAME_LAST}, profile, "de"));
  EXPECT_EQ(base::ASCIIToUTF16("Margaretha"),
            GetLabelName({NAME_MIDDLE}, profile, "de"));
  EXPECT_EQ(base::string16(), GetLabelName({EMPTY_TYPE}, profile, "de"));
  EXPECT_EQ(base::string16(), GetLabelName({}, profile, "de"));
}

}  // namespace
}  // namespace autofill
