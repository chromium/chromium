// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/loyalty_field_parser.h"

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> LoyaltyFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  std::optional<FieldAndMatchInfo> match;
  if (ParseField(context, scanner, "LOYALTY_MEMBERSHIP_ID", &match)) {
    return std::make_unique<LoyaltyFieldParser>(std::move(*match));
  }

  return nullptr;
}

LoyaltyFieldParser::LoyaltyFieldParser(FieldAndMatchInfo match)
    : match_(std::move(match)) {}

void LoyaltyFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, LOYALTY_MEMBERSHIP_ID, kBaseLoyaltyCardParserScore,
                    field_candidates);
}

}  // namespace autofill
