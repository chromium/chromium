// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_MERCHANT_PROMO_CODE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_MERCHANT_PROMO_CODE_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class MerchantPromoCodeSuggestionGenerator : public SuggestionGenerator {
 public:
  MerchantPromoCodeSuggestionGenerator();
  ~MerchantPromoCodeSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormStructure& form,
      const AutofillField& field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<FillingProduct,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormStructure& form,
      const AutofillField& field,
      const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  base::WeakPtr<MerchantPromoCodeSuggestionGenerator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MerchantPromoCodeSuggestionGenerator> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_MERCHANT_PROMO_CODE_SUGGESTION_GENERATOR_H_
