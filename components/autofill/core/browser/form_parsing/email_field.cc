// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/email_field.h"

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormField> EmailField::Parse(AutofillScanner* scanner,
                                             const LanguageCode& page_language,
                                             PatternSource pattern_source,
                                             LogManager* log_manager) {
  AutofillField* field;
  base::span<const MatchPatternRef> email_patterns =
      GetMatchPatterns("EMAIL_ADDRESS", page_language, pattern_source);
  if (ParseFieldSpecifics(scanner, kEmailRe,
                          kDefaultMatchParamsWith<MatchFieldType::kEmail>,
                          email_patterns, &field, {log_manager, "kEmailRe"})) {
    return std::make_unique<EmailField>(field);
  }

  return nullptr;
}

EmailField::EmailField(const AutofillField* field) : field_(field) {}

void EmailField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, EMAIL_ADDRESS, kBaseEmailParserScore,
                    field_candidates);
}

}  // namespace autofill
