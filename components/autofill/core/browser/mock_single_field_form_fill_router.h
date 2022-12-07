// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SINGLE_FIELD_FORM_FILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SINGLE_FIELD_FORM_FILL_ROUTER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class AutofillClient;

class MockSingleFieldFormFillRouter : public SingleFieldFormFillRouter {
 public:
  explicit MockSingleFieldFormFillRouter(
      AutocompleteHistoryManager* autocomplete_history_manager,
      IBANManager* iban_manager,
      MerchantPromoCodeManager* merchant_promo_code_manager);
  ~MockSingleFieldFormFillRouter() override;

  MOCK_METHOD(void,
              OnWillSubmitForm,
              (const FormData& form,
               const FormStructure* form_structure,
               bool is_autocomplete_enabled),
              (override));
  MOCK_METHOD(bool,
              OnGetSingleFieldSuggestions,
              (AutoselectFirstSuggestion autoselect_first_suggestion,
               const FormFieldData& field,
               const AutofillClient& client,
               base::WeakPtr<SingleFieldFormFiller::SuggestionsHandler> handler,
               const SuggestionsContext& context),
              (override));
  MOCK_METHOD(void,
              OnWillSubmitFormWithFields,
              (const std::vector<FormFieldData>& fields,
               bool is_autocomplete_enabled),
              (override));
  MOCK_METHOD(void,
              CancelPendingQueries,
              (const SingleFieldFormFiller::SuggestionsHandler*),
              (override));
  MOCK_METHOD(void,
              OnRemoveCurrentSingleFieldSuggestion,
              (const std::u16string&, const std::u16string&, int),
              (override));
  MOCK_METHOD(void,
              OnSingleFieldSuggestionSelected,
              (const std::u16string&, int),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SINGLE_FIELD_FORM_FILL_ROUTER_H_
