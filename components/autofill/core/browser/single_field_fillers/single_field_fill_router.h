// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_SINGLE_FIELD_FILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_SINGLE_FIELD_FILL_ROUTER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutocompleteHistoryManager;
class AutofillClient;
class FormStructure;
class IbanManager;
class MerchantPromoCodeManager;

// Owned by AutofillClient. Routes single field form filling requests to direct
// them to Autocomplete, merchant promo codes or IBAN.
class SingleFieldFillRouter {
 public:
  // Some single-field fillers return suggestions asynchronously. This callback
  // is used to eventually return suggestions. `field_id` identifies the field
  // the query refer to. `suggestions` is the list of fetched suggestions.
  using OnSuggestionsReturnedCallback =
      base::OnceCallback<void(FieldGlobalId, const std::vector<Suggestion>&)>;

  explicit SingleFieldFillRouter(
      AutocompleteHistoryManager* autocomplete_history_manager,
      IbanManager* iban_manager,
      MerchantPromoCodeManager* merchant_promo_code_manager);
  SingleFieldFillRouter(const SingleFieldFillRouter&) = delete;
  SingleFieldFillRouter& operator=(const SingleFieldFillRouter&) = delete;
  virtual ~SingleFieldFillRouter();

  // Routes every field in a form to its correct single-field filler, calling
  // OnWillSubmitFormWithFields() with the vector of fields for that specific
  // filler. If |form_structure| is not nullptr, then the fields in |form| and
  // |form_structure| should be 1:1. It is possible for |form_structure| to be
  // nullptr while |form| has data, which means there were fields in the form
  // that were not able to be parsed as autofill fields.
  virtual void OnWillSubmitForm(const FormData& form,
                                const FormStructure* form_structure,
                                bool is_autocomplete_enabled);

  // Cancels all pending queries. This is only applicable to single-field
  // fillers that fetch suggestions asynchronously.
  virtual void CancelPendingQueries();

  // If applicable, removes the currently-selected suggestion from the database.
  // `type` is the SuggestionType of the suggestion to be
  // removed.
  virtual void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value,
      SuggestionType type);

  // Invoked when the user selects `suggestion` in the list of suggestions. For
  // Autocomplete, this function logs the DaysSinceLastUse of the Autocomplete
  // entry associated with the value of the suggestion.
  virtual void OnSingleFieldSuggestionSelected(const Suggestion& suggestion);

 private:
  // Handles autocompleting single fields. The `AutocompleteHistoryManager` is
  // a KeyedService that outlives the `SingleFieldFillRouter`.
  raw_ref<AutocompleteHistoryManager> autocomplete_history_manager_;

  // Handles autofilling IBAN fields. Can be null on unsupported platforms, but
  // otherwise outlives the `SingleFieldFillRouter`, since it is a
  // KeyedService.
  raw_ptr<IbanManager> iban_manager_;

  // Handles autofilling merchant promo code fields. Can be null on unsupported
  // platforms, but otherwise outlives the `SingleFieldFillRouter`, since it
  // is a KeyedService.
  raw_ptr<MerchantPromoCodeManager> merchant_promo_code_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_SINGLE_FIELD_FILL_ROUTER_H_
