// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_contact_form_label_formatter.h"

#include <memory>
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
  return {NO_SERVER_DATA,         NAME_FULL,
          EMAIL_ADDRESS,          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2,     ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,      ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,       ADDRESS_HOME_COUNTRY,
          PHONE_HOME_WHOLE_NUMBER};
}

TEST(AddressContactFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FULL, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels().empty());
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"19 North Sq", u"(617) 523-2338",
                                      u"sarah.revere@aol.com"}),
                  ConstructLabelLine({u"151 Irving Ave", u"(617) 514-1600"}),
                  ConstructLabelLine({u"19 North Sq", u"paul1775@gmail.com"}),
                  ConstructLabelLine({u"(617) 324-0000", u"deborah@mit.edu"}),
                  u"Old North Church, 193 Salem St", std::u16string()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"Sarah Revere", u"(617) 523-2338",
                                      u"sarah.revere@aol.com"}),
                  ConstructLabelLine({u"Jackie L Kennedy", u"(617) 514-1600"}),
                  ConstructLabelLine({u"Paul Revere", u"paul1775@gmail.com"}),
                  ConstructLabelLine({u"Deborah Katabi", u"(617) 324-0000",
                                      u"deborah@mit.edu"}),
                  u"", std::u16string()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_CITY, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"19 North Sq", u"(617) 523-2338",
                                      u"sarah.revere@aol.com"}),
                  ConstructLabelLine({u"151 Irving Ave", u"(617) 514-1600"}),
                  ConstructLabelLine({u"19 North Sq", u"paul1775@gmail.com"}),
                  ConstructLabelLine({u"(617) 324-0000", u"deborah@mit.edu"}),
                  u"Old North Church, 193 Salem St", std::u16string()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedEmail) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine(
                      {u"Sarah Revere", u"19 North Sq", u"(617) 523-2338"}),
                  ConstructLabelLine({u"Jackie L Kennedy", u"151 Irving Ave",
                                      u"(617) 514-1600"}),
                  ConstructLabelLine({u"Paul Revere", u"19 North Sq"}),
                  ConstructLabelLine({u"Deborah Katabi", u"(617) 324-0000"}),
                  u"Old North Church, 193 Salem St", std::u16string()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedPhone) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", PHONE_HOME_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"Sarah Revere", u"19 North Sq",
                                      u"sarah.revere@aol.com"}),
                  ConstructLabelLine({u"Jackie L Kennedy", u"151 Irving Ave"}),
                  ConstructLabelLine(
                      {u"Paul Revere", u"19 North Sq", u"paul1775@gmail.com"}),
                  ConstructLabelLine({u"Deborah Katabi", u"deborah@mit.edu"}),
                  u"Old North Church, 193 Salem St", std::u16string()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedName) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", NAME_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({u"Av. Pedro Álvares Cabral, 1301",
                              u"(11) 2648-0254", u"tarsila@aol.com"}),
          ConstructLabelLine({u"Estr. Dona Castorina, 110", u"(21) 98765-0000",
                              u"aavila@uol.com.br"})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"Tarsila do Amaral", u"(11) 2648-0254",
                                      u"tarsila@aol.com"}),
                  ConstructLabelLine({u"Artur Avila", u"(21) 98765-0000",
                                      u"aavila@uol.com.br"})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_ZIP, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({u"Av. Pedro Álvares Cabral, 1301",
                              u"(11) 2648-0254", u"tarsila@aol.com"}),
          ConstructLabelLine({u"Estr. Dona Castorina, 110", u"(21) 98765-0000",
                              u"aavila@uol.com.br"})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedEmail) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine({u"Tarsila do Amaral",
                                              u"Av. Pedro Álvares Cabral, 1301",
                                              u"(11) 2648-0254"}),
                          ConstructLabelLine({u"Artur Avila",
                                              u"Estr. Dona Castorina, 110",
                                              u"(21) 98765-0000"})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedPhone) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", PHONE_HOME_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine({u"Tarsila do Amaral",
                                              u"Av. Pedro Álvares Cabral, 1301",
                                              u"tarsila@aol.com"}),
                          ConstructLabelLine({u"Artur Avila",
                                              u"Estr. Dona Castorina, 110",
                                              u"aavila@uol.com.br"})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForFormWithPartialAddressFields) {
  AutofillProfile profile = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", EMAIL_ADDRESS,
      {NAME_FULL, EMAIL_ADDRESS, ADDRESS_HOME_ZIP, PHONE_HOME_WHOLE_NUMBER});

  // Checks that only address fields in the form are shown in the label.
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine({u"Sarah Revere", u"02113"})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForFormWithoutName_FocusedStreetAddress) {
  AutofillProfile profile1 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  std::vector<AutofillProfile*> profiles{&profile1};
  std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1,
      {ADDRESS_HOME_ZIP, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER});

  // Checks that the name is not in the label and that the phone number is for
  // a unique profile.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"(617) 523-2338"));

  profiles = {&profile1, &profile1};
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     {ADDRESS_HOME_LINE1, ADDRESS_HOME_ZIP,
                                      EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER});

  // Checks that the name is not in the label and that the phone number is for
  // multiple profiles with the same phone number and email address.
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"(617) 523-2338", u"(617) 523-2338"));

  AutofillProfile profile2 = AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Sarah", "", "Revere", "sarah@gmail.com", "",
                       "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  profiles = {&profile1, &profile2};
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     {ADDRESS_HOME_LINE1, ADDRESS_HOME_ZIP,
                                      EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER});
  // Checks that the name is not in the label and that the email address is
  // shown because the profiles' email addresses are different.
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"sarah.revere@aol.com", u"sarah@gmail.com"));
}

}  // namespace
}  // namespace autofill
