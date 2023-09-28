// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_email_form_label_formatter.h"

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

ServerFieldTypeSet GetFieldTypes() {
  return {NAME_FULL,
          EMAIL_ADDRESS,
          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2,
          ADDRESS_HOME_CITY,
          ADDRESS_HOME_STATE,
          ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_ZIP,
          ADDRESS_HOME_COUNTRY};
}

TEST(AddressEmailFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<const AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FULL, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels().empty());
}

TEST(AddressEmailFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "", "", "", "", "", "US", "");

  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "John", "", "Adams", "", "", "", "", "Quincy",
                       "MA", "02169", "US", "");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2,
                                                     &profile3, &profile4};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"333 Washington St", u"jfk@gmail.com"}),
                  u"151 Irving Ave", u"paul1775@gmail.com", std::u16string()));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "", "", "", "paul1775@gmail.com", "", "", "",
                       "", "", "", "US", "");

  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2,
                                                     &profile3, &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John F Kennedy", u"jfk@gmail.com"}),
                  u"Jackie Kennedy", u"paul1775@gmail.com", std::u16string()));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "", "", "", "paul1775@gmail.com", "", "", "",
                       "", "", "", "US", "");

  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "", "", "", "", "", "", "", "Quincy", "MA",
                       "02169", "US", "");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2,
                                                     &profile3, &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_ZIP, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"333 Washington St", u"jfk@gmail.com"}),
                  u"151 Irving Ave", u"paul1775@gmail.com", std::u16string()));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedEmail) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "", "", "Hyannis", "MA", "02601", "US", "");

  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "", "", "", "paul1775@gmail.com", "", "", "",
                       "", "", "", "US", "");

  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2,
                                                     &profile3, &profile4};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John F Kennedy", u"333 Washington St"}),
                  u"Jackie Kennedy", std::u16string(), u"141 Franklin St"));
}

TEST(AddressEmailFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedName) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", NAME_FULL, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine({u"Av. Pedro Álvares Cabral, 1301",
                                              u"tarsila@aol.com"}),
                          ConstructLabelLine({u"Estr. Dona Castorina, 110",
                                              u"aavila@uol.com.br"})));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({u"Tarsila do Amaral", u"tarsila@aol.com"}),
          ConstructLabelLine({u"Artur Avila", u"aavila@uol.com.br"})));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", ADDRESS_HOME_DEPENDENT_LOCALITY, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(ConstructLabelLine({u"Av. Pedro Álvares Cabral, 1301",
                                              u"tarsila@aol.com"}),
                          ConstructLabelLine({u"Estr. Dona Castorina, 110",
                                              u"aavila@uol.com.br"})));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedEmail) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::vector<const AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine(
              {u"Tarsila do Amaral", u"Av. Pedro Álvares Cabral, 1301"}),
          ConstructLabelLine({u"Artur Avila", u"Estr. Dona Castorina, 110"})));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForFormWithAddressFieldsMinusStreetAddress) {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<const AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", EMAIL_ADDRESS,
      {NAME_FULL, EMAIL_ADDRESS, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE});

  // Checks that only address fields in the form are shown in the label.
  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John F Kennedy", u"Brookline, MA"})));
}

TEST(AddressEmailFormLabelFormatterTest, GetLabelsForFormWithoutName) {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  const std::vector<const AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1,
      {ADDRESS_HOME_LINE1, ADDRESS_HOME_ZIP, EMAIL_ADDRESS});

  // Checks that the name does not appear in the labels.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"sarah.revere@aol.com"));
}

}  // namespace
}  // namespace autofill
