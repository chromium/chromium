// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/iban_field_parser.h"

#include <memory>
#include <optional>
#include <utility>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> IbanFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  std::optional<FieldAndMatchInfo> match;
  if (ParseField(context, scanner, "IBAN_VALUE", &match)) {
    return std::make_unique<IbanFieldParser>(std::move(*match));
  }
  return nullptr;
}

IbanFieldParser::IbanFieldParser(FieldAndMatchInfo match)
    : match_(std::move(match)) {}

void IbanFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, IBAN_VALUE, kBaseIbanParserScore, field_candidates);
}

}  // namespace autofill
