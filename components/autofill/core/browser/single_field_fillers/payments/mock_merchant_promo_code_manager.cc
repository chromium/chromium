// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/payments/mock_merchant_promo_code_manager.h"

#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"

namespace autofill {

MockMerchantPromoCodeManager::MockMerchantPromoCodeManager(
    PaymentsDataManager* payments_data_manager)
    : MerchantPromoCodeManager(payments_data_manager,
                               /*is_off_the_record=*/false) {}

MockMerchantPromoCodeManager::~MockMerchantPromoCodeManager() = default;

}  // namespace autofill
