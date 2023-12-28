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
std::unique_ptr<FormField> IbanField::Parse(ParsingContext& context,
                                            AutofillScanner* scanner) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> iban_patterns = GetMatchPatterns(
      IBAN_VALUE, context.page_language, context.pattern_source);

  if (ParseFieldSpecifics(context, scanner, kIbanRe,
                          kDefaultMatchParamsWith<MatchFieldType::kNumber,
                                                  MatchFieldType::kTextArea>,
                          iban_patterns, &field, "kIbanRe")) {
    return std::make_unique<IbanField>(field);
  }

  return nullptr;
}

IbanField::IbanField(const AutofillField* field) : field_(field) {}

void IbanField::AddClassifications(FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, IBAN_VALUE, kBaseIbanParserScore, field_candidates);
}

}  // namespace autofill
