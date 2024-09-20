// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PARSING_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PARSING_TEST_UTILS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_field_parser.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/language_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

enum class ParseResult {
  // The form was successfully parsed and at least one type was assigned.
  kParsed = 0,
  // Not a single type was assigned.
  kNotParsed,
  kMaxValue = kNotParsed
};

class FormFieldParserTestBase {
 public:
  FormFieldParserTestBase();
  FormFieldParserTestBase(const FormFieldParserTestBase&) = delete;
  FormFieldParserTestBase& operator=(const FormFieldParserTestBase&) = delete;
  ~FormFieldParserTestBase();

 protected:
  // Add a field with `control_type`, `name`, `label`, `placeholder`, and
  // `max_length` that is expected to be of `expected_type`.
  void AddFormFieldData(FormControlType control_type,
                        std::string_view name,
                        std::string_view label,
                        std::string_view placeholder,
                        int max_length,
                        FieldType expected_type);

  // Add a field with `control_type`, `name`, and `label` that is expected to be
  // of `expected_type`.
  void AddFormFieldData(FormControlType control_type,
                        std::string_view name,
                        std::string_view label,
                        FieldType expected_type);

  // Convenience wrapper for text control elements.
  void AddTextFormFieldData(std::string_view name,
                            std::string_view label,
                            FieldType expected_type);

  // Convenience wrapper for 'select-one' elements.
  void AddSelectOneFormFieldData(std::string_view name,
                                 std::string_view label,
                                 const std::vector<SelectOption>& options,
                                 FieldType expected_type);

  // Apply parsing and verify the expected types.
  // |parsed| indicates if at least one field could be parsed successfully.
  // |client_country| indicates the assumed country based on the geo ip.
  // |page_language| indicates the language to be used for parsing, default
  // empty value means the language is unknown and patterns of all languages are
  // used.
  void ClassifyAndVerify(
      ParseResult parse_result = ParseResult::kParsed,
      const GeoIpCountryCode& client_country = GeoIpCountryCode(""),
      const LanguageCode& page_language = LanguageCode(""),
      PatternFile pattern_file = *GetActivePatternFile());

  // Runs multiple parsing attempts until the end of the form is reached and
  // verifies the expected types.
  void ClassifyAndVerifyWithMultipleParses(
      const GeoIpCountryCode& client_country = GeoIpCountryCode(""),
      const LanguageCode& page_language = LanguageCode(""));

  // Removes all the fields and resets the expectations.
  void ClearFieldsAndExpectations();

  // Test the parsed verifications against the expectations.
  void TestClassificationExpectations();

  // Apply the parsing with a specific parser.
  virtual std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                 AutofillScanner* scanner) = 0;

  FieldRendererId MakeFieldRendererId();

  // Fields that will be parsed.
  std::vector<std::unique_ptr<AutofillField>> fields_;
  // Actual outcome of parsing.
  FieldCandidatesMap field_candidates_map_;
  // Expectations of parsing.
  std::map<FieldGlobalId, FieldType> expected_classifications_;

 private:
  uint64_t id_counter_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PARSING_TEST_UTILS_H_
