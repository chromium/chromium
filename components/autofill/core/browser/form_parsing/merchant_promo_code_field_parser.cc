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
    AutofillScanner& scanner) {
  std::optional<FieldAndMatchInfo> match;
  if (ParseField(context, scanner, "MERCHANT_PROMO_CODE", &match)) {
    return std::make_unique<MerchantPromoCodeFieldParser>(std::move(*match));
  }
  return nullptr;
}

MerchantPromoCodeFieldParser::MerchantPromoCodeFieldParser(
    FieldAndMatchInfo match)
    : match_(std::move(match)) {}

void MerchantPromoCodeFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, MERCHANT_PROMO_CODE,
                    kBaseMerchantPromoCodeParserScore, field_candidates);
}

}  // namespace autofill
