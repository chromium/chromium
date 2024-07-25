// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/iban_manager.h"
#include "components/autofill/core/browser/single_field_form_filler.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutofillClient;
class AutofillField;
class FormStructure;
class MerchantPromoCodeManager;

// Owned by AutofillClient, and is one per tab. Routes single field form filling
// requests, such as choosing whether to direct them to Autocomplete, merchant
// promo codes or IBAN.
class SingleFieldFormFillRouter : public SingleFieldFormFiller {
 public:
  explicit SingleFieldFormFillRouter(
      AutocompleteHistoryManager* autocomplete_history_manager,
      IbanManager* iban_manager,
      MerchantPromoCodeManager* merchant_promo_code_manager);
  ~SingleFieldFormFillRouter() override;
  SingleFieldFormFillRouter(const SingleFieldFormFillRouter&) = delete;
  SingleFieldFormFillRouter& operator=(const SingleFieldFormFillRouter&) =
      delete;

  // Routes every field in a form to its correct SingleFieldFormFiller, calling
  // SingleFieldFormFiller::OnWillSubmitFormWithFields() with the vector of
  // fields for that specific SingleFieldFormFiller. If |form_structure| is not
  // nullptr, then the fields in |form| and |form_structure| should be 1:1. It
  // is possible for |form_structure| to be nullptr while |form| has data, which
  // means there were fields in the form that were not able to be parsed as
  // autofill fields.
  virtual void OnWillSubmitForm(const FormData& form,
                                const FormStructure* form_structure,
                                bool is_autocomplete_enabled);

  // SingleFieldFormFiller overrides:
  [[nodiscard]] bool OnGetSingleFieldSuggestions(
      const FormStructure* form_structure,
      const FormFieldData& field,
      const AutofillField* autofill_field,
      const AutofillClient& client,
      OnSuggestionsReturnedCallback on_suggestions_returned) override;
  void OnWillSubmitFormWithFields(const std::vector<FormFieldData>& fields,
                                  bool is_autocomplete_enabled) override;
  void CancelPendingQueries() override;
  void OnRemoveCurrentSingleFieldSuggestion(const std::u16string& field_name,
                                            const std::u16string& value,
                                            SuggestionType type) override;
  void OnSingleFieldSuggestionSelected(const Suggestion& suggestion) override;

 private:
  // Handles autocompleting single fields. The `AutocompleteHistoryManager` is
  // a KeyedService that outlives the `SingleFieldFormFillRouter`.
  // TODO(crbug.com/40941458): Once WebView doesn't have an
  // AutocompleteHistoryManager anymore, this should become a raw_ptr instead.
  raw_ref<AutocompleteHistoryManager> autocomplete_history_manager_;

  // Handles autofilling IBAN fields. Can be null on unsupported platforms, but
  // otherwise outlives the `SingleFieldFormFillRouter`, since it is a
  // KeyedService.
  raw_ptr<IbanManager> iban_manager_;

  // Handles autofilling merchant promo code fields. Can be null on unsupported
  // platforms, but otherwise outlives the `SingleFieldFormFillRouter`, since it
  // is a KeyedService.
  raw_ptr<MerchantPromoCodeManager> merchant_promo_code_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_
