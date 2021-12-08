// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/name_field.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class NameFieldTest : public FormFieldTest {
 public:
  NameFieldTest() = default;
  NameFieldTest(const NameFieldTest&) = delete;
  NameFieldTest& operator=(const NameFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("us")) override {
    return NameField::Parse(scanner, page_language, nullptr);
  }
};

TEST_F(NameFieldTest, FirstMiddleLast) {
  AddTextFormFieldData("First Name", "First", NAME_FIRST);
  AddTextFormFieldData("Name Middle", "Middle", NAME_MIDDLE);
  AddTextFormFieldData("Last Name", "Last", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, FirstMiddleLast2) {
  AddTextFormFieldData("firstName", "", NAME_FIRST);
  AddTextFormFieldData("middleName", "", NAME_MIDDLE);
  AddTextFormFieldData("lastName", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Test that a field for a honorific title is parsed correctly.
TEST_F(NameFieldTest, HonorificPrefixFirstLast) {
  // With support for two last names, the parsing should find the first name
  // field and the two last name fields.
  // TODO(crbug.com/1098943): Remove once launched.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  AddTextFormFieldData("salutation", "", NAME_HONORIFIC_PREFIX);
  AddTextFormFieldData("first_name", "", NAME_FIRST);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, FirstLast) {
  AddTextFormFieldData("first_name", "", NAME_FIRST);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, NameSurname) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableNameSurenameParsing);

  AddTextFormFieldData("name", "name", NAME_FIRST);
  AddTextFormFieldData("surename", "surname", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, NameSurname_DE) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableNameSurenameParsing);

  AddTextFormFieldData("name", "name", NAME_FIRST);
  AddTextFormFieldData("nachname", "nachname", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, FirstLast2) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("last_name", "Name", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, FirstLastMiddleWithSpaces) {
  AddTextFormFieldData("fist_name", "First  Name", NAME_FIRST);
  AddTextFormFieldData("middle_name", "Middle  Name", NAME_MIDDLE);
  AddTextFormFieldData("last_name", "Last  Name", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, FirstLastEmpty) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, FirstMiddleLastEmpty) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("middle_name", "", NAME_MIDDLE_INITIAL);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, MiddleInitial) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("middle_name", "MI", NAME_MIDDLE_INITIAL);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(NameFieldTest, MiddleInitialNoLastName) {
  AddTextFormFieldData("first_name", "First Name", UNKNOWN_TYPE);
  AddTextFormFieldData("middle_name", "MI", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

// Tests that a website with a first and second surname field is parsed
// correctly.
TEST_F(NameFieldTest, HonorificPrefixAndFirstNameAndHispanicLastNames) {
  // With support for two last names, the parsing should find the first name
  // field and the two last name fields.
  // TODO(crbug.com/1098943): Remove once launched.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  AddTextFormFieldData("tratamiento", "tratamiento", NAME_HONORIFIC_PREFIX);
  AddTextFormFieldData("nombre", "nombre", NAME_FIRST);
  AddTextFormFieldData("apellido paterno", "apellido_paterno", NAME_LAST_FIRST);
  AddTextFormFieldData("segunda apellido", "segunda_apellido",
                       NAME_LAST_SECOND);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Tests that a website with a first and second surname field is parsed
// correctly.
TEST_F(NameFieldTest, FirstNameAndOptionalMiddleNameAndHispanicLastNames) {
  // With support for two last names, the parsing should find the first name
  // field and the two last name fields.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  AddTextFormFieldData("nombre", "nombre", NAME_FIRST);
  AddTextFormFieldData("middle name", "middle_name", NAME_MIDDLE);
  AddTextFormFieldData("apellido paterno", "apellido_paterno", NAME_LAST_FIRST);
  AddTextFormFieldData("segunda apellido", "segunda_apellido",
                       NAME_LAST_SECOND);

  ClassifyAndVerify(ParseResult::PARSED);
}

// This case is from the dell.com checkout page.  The middle initial "mi" string
// came at the end following other descriptive text.  http://crbug.com/45123.
TEST_F(NameFieldTest, MiddleInitialAtEnd) {
  AddTextFormFieldData("XXXnameXXXfirst", "", NAME_FIRST);
  AddTextFormFieldData("XXXnameXXXmi", "", NAME_MIDDLE_INITIAL);
  AddTextFormFieldData("XXXnameXXXlast", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Test the coverage of all found strings for first and second last names.
TEST_F(NameFieldTest, HispanicLastNameRegexConverage) {
  std::vector<std::u16string> first_last_name_strings = {
      u"Primer apellido", u"apellidoPaterno", u"apellido_paterno",
      u"first_surname",   u"first surname",   u"apellido1"};

  std::vector<std::u16string> second_last_name_strings = {
      u"Segundo apellido", u"apellidoMaterno", u"apellido_materno",
      u"apellido2",        u"second_surname",  u"second surname",
  };

  std::vector<std::u16string> neither_first_or_second_last_name_strings = {
      u"apellido",
      u"apellidos",
  };

  for (const auto& string : first_last_name_strings) {
    SCOPED_TRACE(string);
    EXPECT_TRUE(MatchesPattern(string, kNameLastFirstRe, nullptr));
  }

  for (const auto& string : second_last_name_strings) {
    SCOPED_TRACE(string);
    EXPECT_TRUE(MatchesPattern(string, kNameLastSecondRe, nullptr));
  }

  for (const auto& string : neither_first_or_second_last_name_strings) {
    SCOPED_TRACE(string);
    EXPECT_FALSE(MatchesPattern(string, kNameLastFirstRe, nullptr));
    EXPECT_FALSE(MatchesPattern(string, kNameLastSecondRe, nullptr));
  }
}

// Tests that address name is not misclassified as name or honorific prefix.
TEST_F(NameFieldTest, NotAddressName) {
  AddTextFormFieldData("name", "Identificação do Endereço", UNKNOWN_TYPE);
  AddTextFormFieldData("title", "Adres Adı", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

// Tests that contact name is classified as full name.
TEST_F(NameFieldTest, ContactNameFull) {
  AddTextFormFieldData("contact", "Контактное лицо", NAME_FULL);

  ClassifyAndVerify(ParseResult::PARSED);
}

}  // namespace autofill
