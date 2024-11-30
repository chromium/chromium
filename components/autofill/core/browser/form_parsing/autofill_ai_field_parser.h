// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_AI_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_AI_FIELD_PARSER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

class AutofillScanner;

// Parser to find fields that are eligible for AutofillAi.
class AutofillAiFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);
  explicit AutofillAiFieldParser(FieldAndMatchInfo match);

  AutofillAiFieldParser(const AutofillAiFieldParser&) = delete;
  AutofillAiFieldParser& operator=(const AutofillAiFieldParser&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FieldAndMatchInfo match_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_AI_FIELD_PARSER_H_
