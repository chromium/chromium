// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_contact_form_label_formatter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/strings/string16.h"
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
  return {NO_SERVER_DATA,
          NAME_BILLING_FULL,
          EMAIL_ADDRESS,
          ADDRESS_BILLING_LINE1,
          ADDRESS_BILLING_LINE2,
          ADDRESS_BILLING_DEPENDENT_LOCALITY,
          ADDRESS_BILLING_CITY,
          ADDRESS_BILLING_STATE,
          ADDRESS_BILLING_ZIP,
          ADDRESS_BILLING_COUNTRY,
          PHONE_BILLING_WHOLE_NUMBER};
}

TEST(AddressContactFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_BILLING_FULL, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels().empty());
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_BILLING_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("19 North Sq"),
                              base::ASCIIToUTF16("(617) 523-2338"),
                              base::ASCIIToUTF16("sarah.revere@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("151 Irving Ave"),
                              base::ASCIIToUTF16("(617) 514-1600")}),
          ConstructLabelLine({base::ASCIIToUTF16("19 North Sq"),
                              base::ASCIIToUTF16("paul1775@gmail.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("(617) 324-0000"),
                              base::ASCIIToUTF16("deborah@mit.edu")}),
          base::ASCIIToUTF16("Old North Church, 193 Salem St"),
          base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_BILLING_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("Sarah Revere"),
                              base::ASCIIToUTF16("(617) 523-2338"),
                              base::ASCIIToUTF16("sarah.revere@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Jackie L Kennedy"),
                              base::ASCIIToUTF16("(617) 514-1600")}),
          ConstructLabelLine({base::ASCIIToUTF16("Paul Revere"),
                              base::ASCIIToUTF16("paul1775@gmail.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Deborah Katabi"),
                              base::ASCIIToUTF16("(617) 324-0000"),
                              base::ASCIIToUTF16("deborah@mit.edu")}),
          base::ASCIIToUTF16(""), base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_BILLING_CITY, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("19 North Sq"),
                              base::ASCIIToUTF16("(617) 523-2338"),
                              base::ASCIIToUTF16("sarah.revere@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("151 Irving Ave"),
                              base::ASCIIToUTF16("(617) 514-1600")}),
          ConstructLabelLine({base::ASCIIToUTF16("19 North Sq"),
                              base::ASCIIToUTF16("paul1775@gmail.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("(617) 324-0000"),
                              base::ASCIIToUTF16("deborah@mit.edu")}),
          base::ASCIIToUTF16("Old North Church, 193 Salem St"),
          base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({base::ASCIIToUTF16("Sarah Revere"),
                                      base::ASCIIToUTF16("19 North Sq"),
                                      base::ASCIIToUTF16("(617) 523-2338")}),
                  ConstructLabelLine({base::ASCIIToUTF16("Jackie L Kennedy"),
                                      base::ASCIIToUTF16("151 Irving Ave"),
                                      base::ASCIIToUTF16("(617) 514-1600")}),
                  ConstructLabelLine({base::ASCIIToUTF16("Paul Revere"),
                                      base::ASCIIToUTF16("19 North Sq")}),
                  ConstructLabelLine({base::ASCIIToUTF16("Deborah Katabi"),
                                      base::ASCIIToUTF16("(617) 324-0000")}),
                  base::ASCIIToUTF16("Old North Church, 193 Salem St"),
                  base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedPhone) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Deborah", "", "Katabi", "deborah@mit.edu",
                       "", "", "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4, &profile5, &profile6};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", PHONE_BILLING_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("Sarah Revere"),
                              base::ASCIIToUTF16("19 North Sq"),
                              base::ASCIIToUTF16("sarah.revere@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Jackie L Kennedy"),
                              base::ASCIIToUTF16("151 Irving Ave")}),
          ConstructLabelLine({base::ASCIIToUTF16("Paul Revere"),
                              base::ASCIIToUTF16("19 North Sq"),
                              base::ASCIIToUTF16("paul1775@gmail.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Deborah Katabi"),
                              base::ASCIIToUTF16("deborah@mit.edu")}),
          base::ASCIIToUTF16("Old North Church, 193 Salem St"),
          base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", NAME_BILLING_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine(
              {base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301"),
               base::ASCIIToUTF16("(11) 2648-0254"),
               base::ASCIIToUTF16("tarsila@aol.com")}),
          ConstructLabelLine({base::UTF8ToUTF16("Estr. Dona Castorina, 110"),
                              base::ASCIIToUTF16("(21) 98765-0000"),
                              base::ASCIIToUTF16("aavila@uol.com.br")})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_BILLING_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("Tarsila do Amaral"),
                              base::ASCIIToUTF16("(11) 2648-0254"),
                              base::ASCIIToUTF16("tarsila@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Artur Avila"),
                              base::ASCIIToUTF16("(21) 98765-0000"),
                              base::ASCIIToUTF16("aavila@uol.com.br")})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_BILLING_ZIP, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine(
              {base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301"),
               base::ASCIIToUTF16("(11) 2648-0254"),
               base::ASCIIToUTF16("tarsila@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Estr. Dona Castorina, 110"),
                              base::ASCIIToUTF16("(21) 98765-0000"),
                              base::ASCIIToUTF16("aavila@uol.com.br")})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine(
              {base::ASCIIToUTF16("Tarsila do Amaral"),
               base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301"),
               base::ASCIIToUTF16("(11) 2648-0254")}),
          ConstructLabelLine({base::ASCIIToUTF16("Artur Avila"),
                              base::UTF8ToUTF16("Estr. Dona Castorina, 110"),
                              base::ASCIIToUTF16("(21) 98765-0000")})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedPhone) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", PHONE_BILLING_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine(
              {base::ASCIIToUTF16("Tarsila do Amaral"),
               base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301"),
               base::ASCIIToUTF16("tarsila@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Artur Avila"),
                              base::UTF8ToUTF16("Estr. Dona Castorina, 110"),
                              base::ASCIIToUTF16("aavila@uol.com.br")})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForFormWithPartialAddressFields) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", EMAIL_ADDRESS,
                             {NAME_BILLING_FULL, EMAIL_ADDRESS,
                              ADDRESS_BILLING_ZIP, PHONE_BILLING_WHOLE_NUMBER});

  // Checks that only address fields in the form are shown in the label.
  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine(
          {base::ASCIIToUTF16("Sarah Revere"), base::ASCIIToUTF16("02113")})));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForFormWithoutName_FocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  std::vector<AutofillProfile*> profiles{&profile1};
  std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_BILLING_LINE1,
      {ADDRESS_BILLING_ZIP, EMAIL_ADDRESS, PHONE_BILLING_WHOLE_NUMBER});

  // Checks that the name is not in the label and that the phone number is for
  // a unique profile.
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(base::ASCIIToUTF16("(617) 523-2338")));

  profiles = {&profile1, &profile1};
  formatter =
      LabelFormatter::Create(profiles, "en-US", ADDRESS_BILLING_LINE1,
                             {ADDRESS_BILLING_LINE1, ADDRESS_BILLING_ZIP,
                              EMAIL_ADDRESS, PHONE_BILLING_WHOLE_NUMBER});

  // Checks that the name is not in the label and that the phone number is for
  // multiple profiles with the same phone number and email address.
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(base::ASCIIToUTF16("(617) 523-2338"),
                          base::ASCIIToUTF16("(617) 523-2338")));

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Sarah", "", "Revere", "sarah@gmail.com", "",
                       "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  profiles = {&profile1, &profile2};
  formatter =
      LabelFormatter::Create(profiles, "en-US", ADDRESS_BILLING_LINE1,
                             {ADDRESS_BILLING_LINE1, ADDRESS_BILLING_ZIP,
                              EMAIL_ADDRESS, PHONE_BILLING_WHOLE_NUMBER});
  // Checks that the name is not in the label and that the email address is
  // shown because the profiles' email addresses are different.
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(base::ASCIIToUTF16("sarah.revere@aol.com"),
                          base::ASCIIToUTF16("sarah@gmail.com")));
}

}  // namespace
}  // namespace autofill
