// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/one_time_code_field_parser.h"

#include "base/feature_list.h"

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> OneTimeCodeFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableOneTimeCodeHeuristics)) {
    return nullptr;
  }

  std::optional<FieldAndMatchInfo> match;
  if (ParseField(context, scanner, "ONE_TIME_CODE", &match)) {
    return std::make_unique<OneTimeCodeFieldParser>(std::move(*match));
  }
  return nullptr;
}

OneTimeCodeFieldParser::OneTimeCodeFieldParser(FieldAndMatchInfo match)
    : match_(std::move(match)) {}

void OneTimeCodeFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, ONE_TIME_CODE, kBaseOneTimeCodeParserScore,
                    field_candidates);
}

}  // namespace autofill
