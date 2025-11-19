// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

// Returns the suggestions to be offered on the field in `form` on a
// `trigger_field`. `plus_addresses` are assumed to be the
// plus profiles affiliated with the primary main frame origin.
// TODO(crbug.com/409962888): Remove once the new suggestion generation logic
// is launched.
[[nodiscard]] std::vector<Suggestion> GetSuggestionsFromPlusAddresses(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    bool is_manually_triggered,
    const std::vector<std::string>& plus_addresses);

class PlusAddressSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit PlusAddressSuggestionGenerator(
      AutofillPlusAddressDelegate* plus_address_delegate,
      bool is_manually_triggered);

  ~PlusAddressSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource, std::vector<SuggestionData>>)>
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
  // TODO(crbug.com/409962888): Remove when BrowserAutofillManager doesn't call
  // it anymore.
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
  // Returns the type of suggestion data this generator is supposed to fetch.
  // Returns `std::nullopt` if no suggestion data should be fetched at all.
  std::optional<SuggestionDataSource> GetSourceToSuggest(
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client);

  raw_ptr<AutofillPlusAddressDelegate> plus_address_delegate_;

  bool is_manually_triggered_ = false;

  base::WeakPtrFactory<PlusAddressSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
