// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field_parser.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> MerchantPromoCodeFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> merchant_promo_code_patterns =
      GetMatchPatterns("MERCHANT_PROMO_CODE", context.page_language,
                       context.pattern_file);
  if (ParseField(context, scanner, merchant_promo_code_patterns, &field,
                 "MERCHANT_PROMO_CODE")) {
    return std::make_unique<MerchantPromoCodeFieldParser>(field);
  }
  return nullptr;
}

MerchantPromoCodeFieldParser::MerchantPromoCodeFieldParser(
    const AutofillField* field)
    : field_(field) {}

void MerchantPromoCodeFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, MERCHANT_PROMO_CODE,
                    kBaseMerchantPromoCodeParserScore, field_candidates);
}

}  // namespace autofill
