// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/iban_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

// static
std::unique_ptr<FormField> IBANField::Parse(AutofillScanner* scanner,
                                            const LanguageCode& page_language,
                                            PatternSource pattern_source,
                                            LogManager* log_manager) {
  if (!base::FeatureList::IsEnabled(features::kAutofillParseIBANFields))
    return nullptr;

  AutofillField* field;
  base::span<const MatchPatternRef> iban_patterns =
      GetMatchPatterns(IBAN_VALUE, page_language, pattern_source);

  if (ParseFieldSpecifics(scanner, kIBANRe,
                          kDefaultMatchParamsWith<MatchFieldType::kNumber,
                                                  MatchFieldType::kTextArea>,
                          iban_patterns, &field, {log_manager, "kIBANRe"})) {
    return std::make_unique<IBANField>(field);
  }

  return nullptr;
}

IBANField::IBANField(const AutofillField* field) : field_(field) {}

void IBANField::AddClassifications(FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, IBAN_VALUE, kBaseIBANParserScore, field_candidates);
}

}  // namespace autofill
