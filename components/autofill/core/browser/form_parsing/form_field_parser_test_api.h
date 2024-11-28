// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

class FormFieldParserTestApi {
 public:
  explicit FormFieldParserTestApi(FormFieldParser* parser) : parser_(*parser) {}

  static bool Match(ParsingContext& context,
                    const AutofillField& field,
                    std::u16string_view pattern,
                    DenseSet<MatchAttribute> match_attributes,
                    const char* regex_name = "") {
    return FormFieldParser::Match(context, field, pattern, match_attributes,
                                  regex_name)
        .has_value();
  }

  static bool ParseInAnyOrder(
      AutofillScanner* scanner,
      std::vector<
          std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
          fields_and_parsers) {
    return FormFieldParser::ParseInAnyOrder(scanner, fields_and_parsers);
  }

  void AddClassifications(FieldCandidatesMap& field_candidates) const {
    parser_->AddClassifications(field_candidates);
  }

 private:
  raw_ref<FormFieldParser> parser_;
};

inline FormFieldParserTestApi test_api(FormFieldParser& parser) {
  return FormFieldParserTestApi(&parser);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_TEST_API_H_
