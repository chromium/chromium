// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/numeric_quantity_field_parser.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> NumericQuantityFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> quantity_patterns = GetMatchPatterns(
      "NUMERIC_QUANTITY", context.page_language, context.pattern_source);

  if (ParseFieldSpecifics(
          context, scanner, kNumericQuantityRe,
          kDefaultMatchParamsWith<
              FormControlType::kInputNumber, FormControlType::kSelectOne,
              FormControlType::kSelectList, FormControlType::kTextArea,
              FormControlType::kInputSearch>,
          quantity_patterns, &field, "kNumericQuantityRe")) {
    return base::WrapUnique(new NumericQuantityFieldParser(field));
  }

  return nullptr;
}

NumericQuantityFieldParser::NumericQuantityFieldParser(
    const AutofillField* field)
    : field_(field) {}

void NumericQuantityFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, NUMERIC_QUANTITY, kBaseNumericQuantityParserScore,
                    field_candidates);
}

}  // namespace autofill
