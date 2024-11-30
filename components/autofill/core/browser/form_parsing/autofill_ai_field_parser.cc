// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_ai_field_parser.h"

#include "base/check_op.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> AutofillAiFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  CHECK_EQ(context.pattern_file, PatternFile::kAutofillAi);
  std::optional<FieldAndMatchInfo> match;
  base::span<const MatchPatternRef> patterns = GetMatchPatterns(
      "AUTOFILL_AI", /*language_code=*/std::nullopt, context.pattern_file);
  if (ParseField(context, scanner, patterns, &match, "AUTOFILL_AI")) {
    return std::make_unique<AutofillAiFieldParser>(std::move(*match));
  }
#endif
  return nullptr;
}

AutofillAiFieldParser::AutofillAiFieldParser(FieldAndMatchInfo match)
    : match_(std::move(match)) {}

void AutofillAiFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, IMPROVED_PREDICTION, kBaseImprovedPredictionsScore,
                    field_candidates);
}

}  // namespace autofill
