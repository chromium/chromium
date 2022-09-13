// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_phone_form_label_formatter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
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
  return {NO_SERVER_DATA,     NAME_FULL,          PHONE_HOME_WHOLE_NUMBER,
          ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_CITY,
          ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP,   ADDRESS_HOME_COUNTRY};
}

TEST(AddressPhoneFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FULL, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels().empty());
}

TEST(AddressPhoneFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "", "", "", "", "", "",
                       "", "US", "6175232338");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "John", "", "Adams", "", "", "", "", "", "",
                       "", "US", "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"(617) 730-2000", u"333 Washington St"}),
                  u"151 Irving Ave", u"(617) 523-2338", std::u16string()));
}

TEST(AddressPhoneFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "", "US",
                       "6175232338");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John F Kennedy", u"(617) 730-2000"}),
                  u"Jackie Kennedy", u"(617) 523-2338", std::u16string()));
}

TEST(AddressPhoneFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "", "US",
                       "6175232338");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "", "", "Quincy", "MA",
                       "02169", "US", "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_CITY, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"333 Washington St", u"(617) 730-2000"}),
                  u"151 Irving Ave", u"(617) 523-2338", std::u16string()));
}

TEST(AddressPhoneFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedPhone) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "", "", "", "",
                       "", "", "US", "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "", "", "", "", "", "Paul Revere House",
                       "19 North Square", "Boston", "MA", "02113", "US",
                       "6175232338");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", PHONE_HOME_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John F Kennedy", u"333 Washington St"}),
                  u"Jackie Kennedy", u"Paul Revere House, 19 North Square",
                  std::u16string()));
}

TEST(AddressPhoneFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", NAME_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine(
                      {u"(11) 2648-0254", u"Av. Pedro Álvares Cabral, 1301"}),
                  ConstructLabelLine(
                      {u"(21) 98765-0000", u"Estr. Dona Castorina, 110"})));
}

TEST(AddressPhoneFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"Tarsila do Amaral", u"(11) 2648-0254"}),
                  ConstructLabelLine({u"Artur Avila", u"(21) 98765-0000"})));
}

TEST(AddressPhoneFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_ZIP, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine({u"Av. Pedro Álvares Cabral, 1301",
                                              u"(11) 2648-0254"}),
                          ConstructLabelLine({u"Estr. Dona Castorina, 110",
                                              u"(21) 98765-0000"})));
}

TEST(AddressPhoneFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedPhone) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", PHONE_HOME_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine(
              {u"Tarsila do Amaral", u"Av. Pedro Álvares Cabral, 1301"}),
          ConstructLabelLine({u"Artur Avila", u"Estr. Dona Castorina, 110"})));
}

TEST(AddressPhoneFormLabelFormatterTest,
     GetLabelsForFormWithAddressFieldsMinusStreetAddress) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", PHONE_HOME_WHOLE_NUMBER,
      {NAME_FULL, PHONE_HOME_WHOLE_NUMBER, ADDRESS_HOME_ZIP});

  // Checks that only address fields in the form are shown in the label.
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine({u"John F Kennedy", u"02445"})));
}

TEST(AddressPhoneFormLabelFormatterTest, GetLabelsForFormWithoutName) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1,
      {ADDRESS_HOME_LINE1, ADDRESS_HOME_ZIP, PHONE_HOME_WHOLE_NUMBER});

  // Checks that the name does not appear in the labels.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"(617) 523-2338"));
}

}  // namespace
}  // namespace autofill
