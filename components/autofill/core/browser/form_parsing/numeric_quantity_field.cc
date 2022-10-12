// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/numeric_quantity_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormField> NumericQuantityField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  AutofillField* field;
  base::span<const MatchPatternRef> quantity_patterns =
      GetMatchPatterns("NUMERIC_QUANTITY", page_language, pattern_source);

  if (ParseFieldSpecifics(
          scanner, kNumericQuantityRe,
          kDefaultMatchParamsWith<
              MatchFieldType::kNumber, MatchFieldType::kSelect,
              MatchFieldType::kTextArea, MatchFieldType::kSearch>,
          quantity_patterns, &field, {log_manager, "kNumericQuantityRe"})) {
    return base::WrapUnique(new NumericQuantityField(field));
  }

  return nullptr;
}

NumericQuantityField::NumericQuantityField(const AutofillField* field)
    : field_(field) {}

void NumericQuantityField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, NUMERIC_QUANTITY, kBaseNumericQuantityParserScore,
                    field_candidates);
}

}  // namespace autofill
