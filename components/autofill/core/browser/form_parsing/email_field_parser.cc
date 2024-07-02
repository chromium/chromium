// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/email_field_parser.h"

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> EmailFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> email_patterns = GetMatchPatterns(
      "EMAIL_ADDRESS", context.page_language, context.pattern_source);
  if (ParseField(context, scanner, email_patterns, &field, "EMAIL_ADDRESS")) {
    return std::make_unique<EmailFieldParser>(field);
  }

  return nullptr;
}

EmailFieldParser::EmailFieldParser(const AutofillField* field)
    : field_(field) {}

void EmailFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, EMAIL_ADDRESS, kBaseEmailParserScore,
                    field_candidates);
}

}  // namespace autofill
