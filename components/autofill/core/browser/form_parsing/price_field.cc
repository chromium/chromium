// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormField> PriceField::Parse(
    AutofillScanner* scanner,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> price_patterns =
      GetMatchPatterns("PRICE", page_language, pattern_source);

  if (ParseFieldSpecifics(
          scanner, kPriceRe,
          kDefaultMatchParamsWith<
              MatchFieldType::kNumber, MatchFieldType::kSelect,
              MatchFieldType::kTextArea, MatchFieldType::kSearch>,
          price_patterns, &field, {log_manager, "kPriceRe"})) {
    return std::make_unique<PriceField>(field);
  }

  return nullptr;
}

PriceField::PriceField(const AutofillField* field) : field_(field) {}

void PriceField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, PRICE, kBasePriceParserScore, field_candidates);
}

}  // namespace autofill
