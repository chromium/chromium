// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_TEST_API_H_

#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"

namespace autofill {

class MerchantPromoCodeManagerTestApi {
 public:
  explicit MerchantPromoCodeManagerTestApi(
      MerchantPromoCodeManager& promo_manager)
      : promo_manager_(promo_manager) {}

  void set_most_recent_suggestions_shown_field_global_id(
      FieldGlobalId field_global_id) {
    promo_manager_->uma_recorder_
        .most_recent_suggestions_shown_field_global_id_ = field_global_id;
  }

 private:
  const raw_ref<MerchantPromoCodeManager> promo_manager_;
};

inline MerchantPromoCodeManagerTestApi test_api(
    MerchantPromoCodeManager& promo_manager) {
  return MerchantPromoCodeManagerTestApi(promo_manager);
}

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_TEST_API_H_
