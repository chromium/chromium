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

// SuggestionGenerator is an interface that is used to generate suggestions for
// a specific `FillingProduct`. Each `FillingProduct` has their own
// implementation of the `SuggestionGenerator` interface.
//
// Generating suggestions consists of two phases:
// 1. All generators are called to fetch the data that is gonna be used for
//    creating the suggestions. No assumptions should be made about the order
//    of those calls and some of those calls will be asynchronous.
// 2. Every generator is called again with the data that was fetched for all
//    `FillingProduct`s in step 1, and uses that to generate the suggestions
//    for its specific `FillingProduct`.
//
// Note: Some product suggestions depend on the data from other products.
// E.g. PlusAddresses and Address suggestions needs the data from both
// products, before being able to generate suggestions.
//
// Because of the dependency for each suggestion generator to know about the
// data from other FillingProducts, the generation is split into two phases. The
// second phase is only executed if the first phase finished for all
// `FillingProduct`s. Within each phase there is no guarantee about the order in
// which the suggestion generators are called.
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

  // Obtains data that will be used to generate suggestions on a given trigger
  // `field` that belongs to `form` by calling `GenerateSuggestions` later (See
  // top-level documentation of `SuggestionGenerator` for more details).
  // Once the data is obtained, `callback` is called with the `FillingProduct`
  // of which the data is for and the corresponding `SuggestionData`.
  virtual void FetchSuggestionData(
      const FormStructure& form,
      const AutofillField& field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<FillingProduct,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) = 0;

  // Generates suggestions given `all_suggestion_data` that were fetched by
  // calling `FetchSuggestionData` on all generators (See top-level
  // documentation of `SuggestionGenerator` for more details).
  // Suggestions were triggered on `field` which belongs to `form`. `callback`
  // is called when generation is complete and a list of `Suggestion`
  // objects is passed along with the corresponding `FillingProduct`.
  virtual void GenerateSuggestions(
      const FormStructure& form,
      const AutofillField& field,
      const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) = 0;

 protected:
  // Returns the vector of `SuggestionData` for a specific `FillingProduct`
  // from the `all_suggestion_data` vector.
  std::vector<SuggestionGenerator::SuggestionData>
  ExtractSuggestionDataForFillingProduct(
      const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
          all_suggestion_data,
      FillingProduct filling_product);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_
