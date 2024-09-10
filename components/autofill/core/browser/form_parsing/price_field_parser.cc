// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field_parser.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> PriceFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> price_patterns =
      GetMatchPatterns("PRICE", context.page_language, context.pattern_file);
  if (ParseField(context, scanner, price_patterns, &field, "PRICE")) {
    return std::make_unique<PriceFieldParser>(field);
  }
  return nullptr;
}

PriceFieldParser::PriceFieldParser(const AutofillField* field)
    : field_(field) {}

void PriceFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, PRICE, kBasePriceParserScore, field_candidates);
}

}  // namespace autofill
