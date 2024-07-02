// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_PARSER_H_

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

// A phone number in one of the following formats:
// - area code, prefix, suffix
// - area code, number
// - number
class PhoneFieldParser : public FormFieldParser {
 public:
  ~PhoneFieldParser() override;
  PhoneFieldParser(const PhoneFieldParser&) = delete;
  PhoneFieldParser& operator=(const PhoneFieldParser&) = delete;

  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

#if defined(UNIT_TEST)
  // Assign types to the fields for the testing purposes.
  void AddClassificationsForTesting(
      FieldCandidatesMap& field_candidates_for_testing) const {
    AddClassifications(field_candidates_for_testing);
  }
#endif

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  // This is for easy description of the possible parsing paths of the phone
  // fields.
  enum RegexType {
    REGEX_COUNTRY,
    REGEX_AREA,
    REGEX_AREA_NOTEXT,
    REGEX_PHONE,
    REGEX_PREFIX_SEPARATOR,
    REGEX_PREFIX,
    REGEX_SUFFIX_SEPARATOR,
    REGEX_SUFFIX,
    REGEX_EXTENSION,
  };

  // Parsed fields.
  enum PhonePart {
    FIELD_COUNTRY_CODE,
    FIELD_AREA_CODE,
    FIELD_PHONE,
    FIELD_SUFFIX,
    FIELD_EXTENSION,
    FIELD_MAX,
  };
  using ParsedPhoneFields = std::array<raw_ptr<AutofillField>, FIELD_MAX>;

  explicit PhoneFieldParser(ParsedPhoneFields fields);

  struct Rule {
    RegexType regex;       // The regex used to match this `phone_part`.
    PhonePart phone_part;  // The type/index of the field.
    size_t max_size = 0;   // Max size of the field to match. 0 means any.
  };
  using PhoneGrammar = std::vector<Rule>;

  // Returns all the `PhoneGrammar`s used for parsing.
  static const std::vector<PhoneGrammar>& GetPhoneGrammars();

  // Returns the name of field type which indicated in JSON corresponding to
  // |regex_id|.
  static std::string GetJSONFieldType(RegexType phonetype_id);

  // Convenient wrapper for ParseField().
  static bool ParsePhoneField(ParsingContext& context,
                              AutofillScanner* scanner,
                              raw_ptr<AutofillField>* field,
                              const bool is_country_code_field,
                              const std::string& json_field_type);

  // Tries parsing the given `grammar` into `parsed_fields` and returns true
  // if it succeeded.
  static bool ParseGrammar(ParsingContext& context,
                           const PhoneGrammar& grammar,
                           ParsedPhoneFields& parsed_fields,
                           AutofillScanner* scanner);

  // Returns true if |scanner| points to a <select> field that appears to be the
  // phone country code by looking at its option contents.
  // "Augmented" refers to the fact that we are looking for select options that
  // contain not only a country code but also further text like "Germany (+49)".
  static bool LikelyAugmentedPhoneCountryCode(AutofillScanner* scanner,
                                              raw_ptr<AutofillField>* match);

  // FIELD_PHONE is always present; holds suffix if prefix is present.
  // The rest could be NULL.
  ParsedPhoneFields parsed_phone_fields_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_PARSER_H_
