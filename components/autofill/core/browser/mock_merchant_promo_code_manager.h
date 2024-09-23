// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_MERCHANT_PROMO_CODE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_MERCHANT_PROMO_CODE_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/merchant_promo_code_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockMerchantPromoCodeManager : public MerchantPromoCodeManager {
 public:
  MockMerchantPromoCodeManager();
  ~MockMerchantPromoCodeManager() override;

  MOCK_METHOD(bool,
              OnGetSingleFieldSuggestions,
              (const FormStructure* form_structure,
               const FormFieldData& field,
               const AutofillField* autofill_field,
               const AutofillClient& client,
               SingleFieldFormFiller::OnSuggestionsReturnedCallback callback),
              (override));
  MOCK_METHOD(void,
              OnWillSubmitFormWithFields,
              (const std::vector<FormFieldData>& fields,
               bool is_autocomplete_enabled),
              (override));
  MOCK_METHOD(void, CancelPendingQueries, (), (override));
  MOCK_METHOD(void,
              OnRemoveCurrentSingleFieldSuggestion,
              (const std::u16string&, const std::u16string&, SuggestionType),
              (override));
  MOCK_METHOD(void,
              OnSingleFieldSuggestionSelected,
              (const Suggestion& suggestion),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_MERCHANT_PROMO_CODE_MANAGER_H_
