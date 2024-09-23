// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/name_field_parser.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

bool MatchesPattern(std::u16string_view input, std::string_view pattern_name) {
  static base::NoDestructor<AutofillRegexCache> cache(ThreadSafe(true));
  base::span<const MatchPatternRef> patterns = GetMatchPatterns(
      pattern_name, /*language_code=*/std::nullopt, *GetActivePatternFile());
  return std::ranges::any_of(patterns, [&](MatchPatternRef pattern_ref) {
    return MatchesRegex(
        input, *cache->GetRegexPattern((*pattern_ref).positive_pattern));
  });
}

class NameFieldParserTest : public FormFieldParserTestBase,
                            public testing::Test {
 public:
  NameFieldParserTest() = default;
  NameFieldParserTest(const NameFieldParserTest&) = delete;
  NameFieldParserTest& operator=(const NameFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return NameFieldParser::Parse(context, scanner);
  }
};

TEST_F(NameFieldParserTest, FirstMiddleLast) {
  AddTextFormFieldData("First", "First Name", NAME_FIRST);
  AddTextFormFieldData("Middle", "Name Middle", NAME_MIDDLE);
  AddTextFormFieldData("Last", "Last Name", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, FirstMiddleLast2) {
  AddTextFormFieldData("firstName", "", NAME_FIRST);
  AddTextFormFieldData("middleName", "", NAME_MIDDLE);
  AddTextFormFieldData("lastName", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Test that a field for a honorific title is parsed correctly.
TEST_F(NameFieldParserTest, HonorificPrefixFirstLast) {
  AddTextFormFieldData("salutation", "", NAME_HONORIFIC_PREFIX);
  AddTextFormFieldData("first_name", "", NAME_FIRST);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, FirstLast) {
  AddTextFormFieldData("first_name", "", NAME_FIRST);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, NameSurname) {
  AddTextFormFieldData("name", "name", NAME_FIRST);
  AddTextFormFieldData("surname", "surname", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, NameSurnameWithMiddleName) {
  AddTextFormFieldData("name", "name", NAME_FIRST);
  AddTextFormFieldData("middlename", "middlename", NAME_MIDDLE);
  AddTextFormFieldData("surname", "surname", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, NameSurname_DE) {
  AddTextFormFieldData("name", "name", NAME_FIRST);
  AddTextFormFieldData("nachname", "nachname", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, FirstLast2) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("last_name", "Name", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, FirstLastMiddleWithSpaces) {
  AddTextFormFieldData("fist_name", "First  Name", NAME_FIRST);
  AddTextFormFieldData("middle_name", "Middle  Name", NAME_MIDDLE);
  AddTextFormFieldData("last_name", "Last  Name", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, FirstLastEmpty) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, FirstMiddleLastEmpty) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("middle_name", "", NAME_MIDDLE_INITIAL);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, MiddleInitial) {
  AddTextFormFieldData("first_name", "Name", NAME_FIRST);
  AddTextFormFieldData("middle_initial", "MI", NAME_MIDDLE_INITIAL);
  AddTextFormFieldData("last_name", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(NameFieldParserTest, MiddleInitialNoLastName) {
  AddTextFormFieldData("first_name", "First Name", UNKNOWN_TYPE);
  AddTextFormFieldData("middle_initial", "MI", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

// Tests that a website with a first and second surname field is parsed
// correctly.
TEST_F(NameFieldParserTest, HonorificPrefixAndFirstNameAndHispanicLastNames) {
  AddTextFormFieldData("tratamiento", "tratamiento", NAME_HONORIFIC_PREFIX);
  AddTextFormFieldData("nombre", "nombre", NAME_FIRST);
  AddTextFormFieldData("apellido paterno", "apellido_paterno", NAME_LAST_FIRST);
  AddTextFormFieldData("segunda apellido", "segunda_apellido",
                       NAME_LAST_SECOND);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Tests that a website with a first and second surname field is parsed
// correctly.
TEST_F(NameFieldParserTest,
       FirstNameAndOptionalMiddleNameAndHispanicLastNames) {
  AddTextFormFieldData("nombre", "nombre", NAME_FIRST);
  AddTextFormFieldData("middle_name", "middle name", NAME_MIDDLE);
  AddTextFormFieldData("apellido_paterno", "apellido paterno", NAME_LAST_FIRST);
  AddTextFormFieldData("segunda_apellido", "segunda apellido",
                       NAME_LAST_SECOND);

  ClassifyAndVerify(ParseResult::kParsed);
}

// This case is from the dell.com checkout page.  The middle initial "mi" string
// came at the end following other descriptive text.  http://crbug.com/45123.
TEST_F(NameFieldParserTest, MiddleInitialAtEnd) {
  AddTextFormFieldData("XXXnameXXXfirst", "", NAME_FIRST);
  AddTextFormFieldData("XXXnameXXXmi", "", NAME_MIDDLE_INITIAL);
  AddTextFormFieldData("XXXnameXXXlast", "", NAME_LAST);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Test the coverage of all found strings for first and second last names.
TEST_F(NameFieldParserTest, HispanicLastNameRegexConverage) {
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
    EXPECT_TRUE(MatchesPattern(string, "LAST_NAME_FIRST"));
  }

  for (const auto& string : second_last_name_strings) {
    SCOPED_TRACE(string);
    EXPECT_TRUE(MatchesPattern(string, "LAST_NAME_SECOND"));
  }

  for (const auto& string : neither_first_or_second_last_name_strings) {
    SCOPED_TRACE(string);
    EXPECT_FALSE(MatchesPattern(string, "LAST_NAME_FIRST"));
    EXPECT_FALSE(MatchesPattern(string, "LAST_NAME_SECOND"));
  }
}

// Tests that address name is not misclassified as name or honorific prefix.
TEST_F(NameFieldParserTest, NotAddressName) {
  AddTextFormFieldData("name", "Identificação do Endereço", UNKNOWN_TYPE);
  AddTextFormFieldData("title", "Adres Adı", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

// Tests that contact name is classified as full name.
TEST_F(NameFieldParserTest, ContactNameFull) {
  AddTextFormFieldData("contact", "Контактное лицо", NAME_FULL);

  ClassifyAndVerify(ParseResult::kParsed);
}

}  // namespace

}  // namespace autofill
