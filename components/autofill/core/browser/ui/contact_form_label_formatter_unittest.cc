// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/contact_form_label_formatter.h"

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

ServerFieldTypeSet GetNamePhoneAndEmailFieldTypes() {
  return {NAME_FIRST, NAME_LAST, PHONE_HOME_WHOLE_NUMBER, EMAIL_ADDRESS};
}

TEST(ContactFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_FIRST, GetNamePhoneAndEmailFieldTypes());
  EXPECT_TRUE(formatter->GetLabels().empty());
}

TEST(ContactFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "", "", "19 N Square",
                       "", "Boston", "MA", "02113", "US", "+1 (617) 523-2338");

  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "John", "", "Adams", "", "",
                       "141 Franklin St.", "", "Quincy", "MA", "02169", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_LAST, GetNamePhoneAndEmailFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"(617) 730-2000", u"jfk@gmail.com"}),
                  u"jackie@outlook.com", u"(617) 523-2338", std::u16string()));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedEmail) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "", "", "19 N Square",
                       "", "Boston", "MA", "02113", "US", "+1 (617) 523-2338");

  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St.", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", EMAIL_ADDRESS, GetNamePhoneAndEmailFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"John Kennedy", u"(617) 730-2000"}),
                  u"Jackie Kennedy",
                  ConstructLabelLine({u"Paul Revere", u"(617) 523-2338"}),
                  std::u16string()));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedPhone) {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "", "", "19 N Square",
                       "", "Boston", "MA", "02113", "US", "+1 (617) 523-2338");

  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St.", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", PHONE_HOME_WHOLE_NUMBER,
                             GetNamePhoneAndEmailFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({u"John Kennedy", u"jfk@gmail.com"}),
          ConstructLabelLine({u"Jackie Kennedy", u"jackie@outlook.com"}),
          u"Paul Revere", std::u16string()));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedName) {
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

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", NAME_LAST, GetNamePhoneAndEmailFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(
          ConstructLabelLine({u"(11) 2648-0254", u"tarsila@aol.com"}),
          ConstructLabelLine({u"(21) 98765-0000", u"aavila@uol.com.br"})));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedEmail) {
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

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "pt-BR", EMAIL_ADDRESS, GetNamePhoneAndEmailFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"Tarsila Amaral", u"(11) 2648-0254"}),
                  ConstructLabelLine({u"Artur Avila", u"(21) 98765-0000"})));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedPhone) {
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

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "pt-BR", PHONE_HOME_WHOLE_NUMBER,
                             GetNamePhoneAndEmailFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(ConstructLabelLine({u"Tarsila Amaral", u"tarsila@aol.com"}),
                  ConstructLabelLine({u"Artur Avila", u"aavila@uol.com.br"})));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndPhoneWithFocusedName) {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", NAME_LAST,
                             {NAME_FIRST, NAME_LAST, PHONE_HOME_WHOLE_NUMBER});

  // Checks that the email address is excluded when the form does not contain an
  // email field.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"(617) 730-2000"));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndPhoneWithFocusedPhone) {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create(profiles, "en-US", PHONE_HOME_WHOLE_NUMBER,
                             {NAME_FIRST, NAME_LAST, PHONE_HOME_WHOLE_NUMBER});

  // Checks that the email address is excluded when the form does not contain an
  // email field.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"John Kennedy"));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndEmailWithFocusedName) {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_LAST, {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS});

  // Checks that the phone number is excluded when the form does not contain a
  // phone field.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"jfk@gmail.com"));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndEmailWithFocusedEmail) {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", EMAIL_ADDRESS, {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS});

  // Checks that the phone number is excluded when the form does not contain a
  // phone field.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"John Kennedy"));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForFormWithoutName) {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", PHONE_HOME_COUNTRY_CODE,
      {EMAIL_ADDRESS, PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_AND_NUMBER});

  // Checks that the name does not appear in the labels.
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"sarah.revere@aol.com"));
}

}  // namespace
}  // namespace autofill
