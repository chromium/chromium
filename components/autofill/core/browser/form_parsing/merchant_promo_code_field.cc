// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormField> MerchantPromoCodeField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> merchant_promo_code_patterns =
      GetMatchPatterns("MERCHANT_PROMO_CODE", page_language, pattern_source);

  if (ParseFieldSpecifics(scanner, kMerchantPromoCodeRe,
                          kDefaultMatchParamsWith<MatchFieldType::kNumber,
                                                  MatchFieldType::kTextArea>,
                          merchant_promo_code_patterns, &field,
                          {log_manager, "kMerchantPromoCodeRe"})) {
    return std::make_unique<MerchantPromoCodeField>(field);
  }

  return nullptr;
}

MerchantPromoCodeField::MerchantPromoCodeField(const AutofillField* field)
    : field_(field) {}

void MerchantPromoCodeField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, MERCHANT_PROMO_CODE,
                    kBaseMerchantPromoCodeParserScore, field_candidates);
}

}  // namespace autofill
