// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/prediction_improvements_field_parser.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> PredictionImprovementsFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  if (context.pattern_file != PatternFile::kPredictionImprovements) {
    CHECK(false);
    return nullptr;
  }
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> patterns = GetMatchPatterns(
      "PREDICTION_IMPROVEMENTS", std::nullopt, context.pattern_file);
  if (ParseField(context, scanner, patterns, &field,
                 "PREDICTION_IMPROVEMENTS")) {
    return std::make_unique<PredictionImprovementsFieldParser>(field);
  }
#endif
  return nullptr;
}

PredictionImprovementsFieldParser::PredictionImprovementsFieldParser(
    const AutofillField* field)
    : field_(field) {}

void PredictionImprovementsFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, IMPROVED_PREDICTION, kBaseImprovedPredictionsScore,
                    field_candidates);
}

}  // namespace autofill
