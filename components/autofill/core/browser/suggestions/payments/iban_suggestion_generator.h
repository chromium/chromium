// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_IBAN_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_IBAN_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class IbanSuggestionGenerator : public SuggestionGenerator {
 public:
  IbanSuggestionGenerator();
  ~IbanSuggestionGenerator() override;

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

  base::WeakPtr<IbanSuggestionGenerator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

private:
 // Filter out IBAN-based suggestions based on the following criteria:
 // For local IBANs: Filter out the IBAN value which does not starts with the
 // provided `field_value`.
 // For server IBANs: Filter out IBAN suggestion if any of the following
 // conditions are satisfied:
 // 1. If the IBAN's `prefix` is absent and the length of the `field_value` is
 // less than `kFieldLengthLimitOnServerIbanSuggestion` characters.
 // 2. If the IBAN's prefix is present and prefix matches the `field_value`.
 void FilterIbansToSuggest(const std::u16string& field_value,
                           std::vector<Iban>& ibans);

 base::WeakPtrFactory<IbanSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_IBAN_SUGGESTION_GENERATOR_H_
