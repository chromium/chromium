// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

class GURL;

namespace autofill {

class ValuablesDataManager;

// Generates loyalty card suggestions for the value of trigger `field` and the
// last committed primary main frame URL obtained from `client`. Loyalty cards
// are extracted from the `ValuablesDataManager` using `client`.
// TODO(crbug.com/409962888): Remove after new suggestion generation logic is
// launched.
std::vector<Suggestion> GetSuggestionsForLoyaltyCards(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const PasswordFormClassification& password_form_classification,
    const AutofillClient& client);

// Extends `email_suggestions` with loyalty cards suggestions.
// TODO(crbug.com/409962888): Remove after new suggestion generation logic is
// launched.
void MergeLoyaltyCardsAndAddressSuggestions(
    std::vector<Suggestion>& email_suggestions,
    std::vector<Suggestion> loyalty_card_suggestions);

// Creates loyalty card suggestions specifically for the merge with the email
// suggestions. We cannot use GetLoyaltyCardSuggestions() here because merging
// requires constructing the suggestions directly, rather than using the full
// LoyaltyCardSuggestionGenerator flow.
// TODO(crbug.com/409962888): Remove after new suggestion generation logic is
// launched.
std::vector<Suggestion> CreateLoyaltyCardSuggestionsForMerge(
    const ValuablesDataManager& valuables_manager,
    const GURL& url);

class LoyaltyCardSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit LoyaltyCardSuggestionGenerator(
      const PasswordFormClassification& password_form_classification);
  ~LoyaltyCardSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::FunctionRef<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback);

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

 private:
  // Contains the password form classification for which the generator is
  // currently building suggestions.
  PasswordFormClassification password_form_classification_;

  base::WeakPtrFactory<LoyaltyCardSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
