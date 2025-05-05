// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

class SuggestionGenerator {
 public:
  SuggestionGenerator() = default;
  virtual ~SuggestionGenerator() = default;

  using ReturnedSuggestions =
      std::pair<FillingProduct, std::vector<Suggestion>>;
  // Contains the structures used in order to generate various kind of
  // suggestions.
  using SuggestionData =
      std::variant<EntityInstance, AutofillProfile, CreditCard, Iban>;

  // Obtains data for suggestions for a specific `FillingProduct` and calls
  // `callback` with that product along with the suggestions data.
  virtual void FetchSuggestionData(
      const FormStructure& form,
      const AutofillField& trigger_field,
      AutofillClient& client,
      base::OnceCallback<
          void(std::pair<FillingProduct,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) = 0;

  // Generates suggestions for a specific `FillingProduct` and calls `callback`
  // with that product along with the suggestions that were generated.
  virtual void GenerateSuggestions(
      const FormStructure& form,
      const AutofillField& trigger_field,
      AutofillClient& client,
      const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
          suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) = 0;

  protected:
   // Returns the vector of `SuggestionData` for a specific `FillingProduct`
   // from the `suggestion_data` vector.
   std::vector<SuggestionGenerator::SuggestionData>
   GetSuggestionDataForFillingProduct(
       const std::vector<
           std::pair<FillingProduct, std::vector<SuggestionData>>>&
           suggestion_data,
       FillingProduct filling_product);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_
