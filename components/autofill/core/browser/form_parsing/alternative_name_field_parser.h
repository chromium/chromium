// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ALTERNATIVE_NAME_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ALTERNATIVE_NAME_FIELD_PARSER_H_

#include <memory>

#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

class AutofillScanner;

// A form field parser that can parse either an AlternativeFullNameField or a
// a pair of AlternativeGivenNameField and AlternativeFamilyNameField.
// Currently not implemented and should not be used.
class AlternativeNameFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  AlternativeNameFieldParser(const AlternativeNameFieldParser&) = delete;
  AlternativeNameFieldParser& operator=(const AlternativeNameFieldParser&) =
      delete;

 protected:
  AlternativeNameFieldParser() = default;

  void AddClassifications(FieldCandidatesMap& field_candidates) const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ALTERNATIVE_NAME_FIELD_PARSER_H_
