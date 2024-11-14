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
  std::optional<FieldAndMatchInfo> match;
  base::span<const MatchPatternRef> price_patterns =
      GetMatchPatterns("PRICE", context.page_language, context.pattern_file);
  if (ParseField(context, scanner, price_patterns, &match, "PRICE")) {
    return std::make_unique<PriceFieldParser>(std::move(*match));
  }
  return nullptr;
}

PriceFieldParser::PriceFieldParser(FieldAndMatchInfo match)
    : match_(std::move(match)) {}

void PriceFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, PRICE, kBasePriceParserScore, field_candidates);
}

}  // namespace autofill
