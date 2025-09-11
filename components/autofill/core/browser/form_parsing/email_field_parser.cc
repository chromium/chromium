// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/email_field_parser.h"

#include "base/feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> EmailFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  std::optional<FieldAndMatchInfo> match;
  const AutofillScanner::Position saved_cursor = scanner.GetPosition();
  // Try parsing an email field.
  const bool parsed_email =
      ParseField(context, scanner, "EMAIL_ADDRESS", &match);
  if (parsed_email) {
    // Try parsing the same field as a loyalty card field.
    scanner.Restore(saved_cursor);
    const bool parsed_loyalty_card =
        base::FeatureList::IsEnabled(
            features::kAutofillEnableEmailOrLoyaltyCardsFilling) &&
        ParseField(context, scanner, "LOYALTY_MEMBERSHIP_ID", &match);
    if (parsed_loyalty_card) {
      return std::make_unique<EmailFieldParser>(std::move(*match),
                                                EMAIL_OR_LOYALTY_MEMBERSHIP_ID);
    }
    scanner.Advance();
    return std::make_unique<EmailFieldParser>(std::move(*match), EMAIL_ADDRESS);
  }

  // TODO(crbug.com/361560365): Consider moving this into the JSON files once
  // this is launched and they support placeholders.
  const FormFieldData& field = scanner.Cursor();
  if ((IsValidEmailAddress(field.placeholder()) ||
       IsValidEmailAddress(field.label()))) {
    scanner.Advance();
    // Since this is either a placeholder or a label match, it's technically not
    // necessarily a high quality label match. However, since this logic
    // predates the low/high quality label distinction, its behavior was kept.
    return std::make_unique<EmailFieldParser>(
        FieldAndMatchInfo(&field,
                          {.matched_attribute =
                               MatchInfo::MatchAttribute::kHighQualityLabel}),
        EMAIL_ADDRESS);
  }

  return nullptr;
}

EmailFieldParser::EmailFieldParser(FieldAndMatchInfo match,
                                   FieldType email_type)
    : match_(std::move(match)), email_type_(email_type) {}

void EmailFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, email_type_, kBaseEmailParserScore,
                    field_candidates);
}

}  // namespace autofill
