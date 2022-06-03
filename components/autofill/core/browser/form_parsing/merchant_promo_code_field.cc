// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

// static
std::unique_ptr<FormField> MerchantPromoCodeField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    LogManager* log_manager) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillParseMerchantPromoCodeFields)) {
    return nullptr;
  }

  AutofillField* field;
  const std::vector<MatchingPattern>& merchant_promo_code_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("MERCHANT_PROMO_CODE",
                                                      page_language);

  if (ParseFieldSpecifics(scanner, kMerchantPromoCodeRe,
                          MATCH_DEFAULT | MATCH_NUMBER | MATCH_TEXT_AREA,
                          merchant_promo_code_patterns, &field,
                          {log_manager, "kMerchantPromoCodeRe"})) {
    return std::make_unique<MerchantPromoCodeField>(field);
  }

  return nullptr;
}

MerchantPromoCodeField::MerchantPromoCodeField(const AutofillField* field)
    : field_(field) {}

void MerchantPromoCodeField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, MERCHANT_PROMO_CODE,
                    kBaseMerchantPromoCodeParserScore, field_candidates);
}

}  // namespace autofill
