// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_form_label_formatter.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace autofill {
namespace {

std::vector<ServerFieldType> GetFieldTypes() {
  return {NO_SERVER_DATA,     NAME_FIRST,
          NAME_LAST,          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2, ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,  ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,   ADDRESS_HOME_COUNTRY};
}

TEST(AddressFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FIRST, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels().empty());
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "", "", "jackie@outlook.com", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "", "US",
                       "5087717796");

  AutofillProfile profile3 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "", "", "", "", "", "US", "");

  AutofillProfile profile4 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John Kennedy", u"Brookline, MA 02445"}),
                  u"Hyannis, MA", u"Paul Revere", std::u16string()));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "", "", "jackie@outlook.com", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "", "US",
                       "5087717796");

  AutofillProfile profile3 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "", "", "", "", "", "US", "");

  AutofillProfile profile4 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_CITY, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John Kennedy", u"333 Washington St"}),
                  u"151 Irving Ave", u"Paul Revere", std::u16string()));
}

TEST(AddressFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "5087717796");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FIRST, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"333 Washington St, Brookline, MA 02445",
                          u"151 Irving Ave, Hyannis, MA 02601"));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedStreetAddress) {
  AutofillProfile profile = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine(
          {u"Tarsila Amaral", u"Vila Mariana, São Paulo-SP, 04094-050"})));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_ZIP, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine(
                  {u"Tarsila Amaral", u"Av. Pedro Álvares Cabral, 1301"})));
}

TEST(AddressFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedName) {
  AutofillProfile profile = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", NAME_FIRST, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"Av. Pedro Álvares Cabral, 1301, Vila Mariana, São "
                          u"Paulo-SP, 04094-050"));
}

TEST(AddressFormLabelFormatterTest, GetLabelsForFormWithoutName) {
  AutofillProfile profile = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1,
      {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_DEPENDENT_LOCALITY,
       ADDRESS_HOME_ZIP});

  // Checks that the name does not appear in the labels.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"Boston, MA 02113"));
}

}  // namespace
}  // namespace autofill
