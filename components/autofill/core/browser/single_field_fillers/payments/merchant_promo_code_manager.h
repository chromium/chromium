// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class AutofillClient;

// Per-profile Merchant Promo Code Manager. This class handles promo code
// related functionality such as retrieving promo code offer data, managing
// promo code suggestions, filling promo code fields, and handling form
// submission data when there is a merchant promo code field present.
class MerchantPromoCodeManager : public KeyedService {
 public:
  MerchantPromoCodeManager();

  MerchantPromoCodeManager(const MerchantPromoCodeManager&) = delete;
  MerchantPromoCodeManager& operator=(const MerchantPromoCodeManager&) = delete;

  ~MerchantPromoCodeManager() override;

  // May generate promo code suggestions for the given `autofill_field` which
  // belongs to the `form_structure`.
  // If `OnGetSingleFieldSuggestions` decides to claim the opportunity to fill
  // `field`, it returns true and calls `on_suggestions_returned`. Claiming the
  // opportunity is not a promise that suggestions will be available. The
  // callback may be called with no suggestions.
  [[nodiscard]] virtual bool OnGetSingleFieldSuggestions(
      const FormStructure& form_structure,
      const FormFieldData& field,
      const AutofillField& autofill_field,
      const AutofillClient& client,
      SingleFieldFillRouter::OnSuggestionsReturnedCallback&
          on_suggestions_returned);

  virtual void OnSingleFieldSuggestionSelected(const Suggestion& suggestion) {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_H_
