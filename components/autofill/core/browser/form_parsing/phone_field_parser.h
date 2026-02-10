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
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/phone_number.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

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
                                                AutofillScanner& scanner);

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
  using ParsedPhoneFields =
      std::array<std::optional<FieldAndMatchInfo>, FIELD_MAX>;

  explicit PhoneFieldParser(ParsedPhoneFields fields);

  struct Rule {
    RegexType regex;        // The regex used to match this `phone_part`.
    PhonePart phone_part;   // The type/index of the field.
    size_t max_length = 0;  // FormFieldData::max_length of the field to match.
                            // 0 means any.
  };

  struct PhoneGrammar {
    PhoneGrammar(std::vector<Rule> rules, size_t id);
    PhoneGrammar(const PhoneGrammar&);
    PhoneGrammar(PhoneGrammar&&);
    PhoneGrammar& operator=(const PhoneGrammar& other);
    PhoneGrammar& operator=(PhoneGrammar&& other);
    ~PhoneGrammar();

    std::vector<Rule> rules;
    // Numerical identifier of grammars used for metrics logging, grammars
    // should not be renumbered and values should not be reused.
    size_t id;
  };

  // Returns all the `PhoneGrammar`s used for parsing.
  static const std::vector<PhoneGrammar>& GetPhoneGrammars();

  // Returns the name of field type which indicated in JSON corresponding to
  // `regex_type`.
  static std::string_view GetJSONFieldType(RegexType regex_type);

  // Convenient wrapper for ParseField().
  static bool ParsePhoneField(ParsingContext& context,
                              AutofillScanner& scanner,
                              std::optional<FieldAndMatchInfo>* match,
                              const bool is_country_code_field,
                              std::string_view json_field_type);

  // Tries parsing the given `grammar` into `parsed_fields` and returns true
  // if it succeeded.
  static bool ParseGrammar(ParsingContext& context,
                           const PhoneGrammar& grammar,
                           ParsedPhoneFields& parsed_fields,
                           AutofillScanner& scanner,
                           bool improve_phone_field_parser_experiment_enabled,
                           bool new_augmented_cc_regex_experiment_enabled);

  // FIELD_PHONE is always present if a match is found. The rest may be nullopt.
  ParsedPhoneFields parsed_phone_fields_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_PARSER_H_
