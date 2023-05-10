// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/iban_manager.h"
#include "components/autofill/core/browser/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/single_field_form_filler.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutofillClient;
class FormStructure;
class MerchantPromoCodeManager;
struct SuggestionsContext;

// Owned by AutofillClient, and is one per tab. Routes single field form filling
// requests, such as choosing whether to direct them to Autocomplete, merchant
// promo codes or IBAN.
class SingleFieldFormFillRouter : public SingleFieldFormFiller {
 public:
  explicit SingleFieldFormFillRouter(
      AutocompleteHistoryManager* autocomplete_history_manager,
      IBANManager* iban_manager,
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
      AutoselectFirstSuggestion autoselect_first_suggestion,
      const FormFieldData& field,
      const AutofillClient& client,
      base::WeakPtr<SingleFieldFormFiller::SuggestionsHandler> handler,
      const SuggestionsContext& context) override;
  void OnWillSubmitFormWithFields(const std::vector<FormFieldData>& fields,
                                  bool is_autocomplete_enabled) override;
  void CancelPendingQueries(
      const SingleFieldFormFiller::SuggestionsHandler* handler) override;
  void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value,
      Suggestion::FrontendId frontend_id) override;
  void OnSingleFieldSuggestionSelected(
      const std::u16string& value,
      Suggestion::FrontendId frontend_id) override;

 private:
  // Handles autocompleting single fields.
  base::WeakPtr<AutocompleteHistoryManager> autocomplete_history_manager_;

  // Handles autofilling IBAN fields (can be null for unsupported platforms).
  base::WeakPtr<IBANManager> iban_manager_;

  // Handles autofilling merchant promo code fields (can be null for unsupported
  // platforms).
  base::WeakPtr<MerchantPromoCodeManager> merchant_promo_code_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_
