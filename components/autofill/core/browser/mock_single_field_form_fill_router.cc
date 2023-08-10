// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"

namespace autofill {

MockSingleFieldFormFillRouter::MockSingleFieldFormFillRouter(
    AutocompleteHistoryManager* autocomplete_history_manager,
    IbanManager* iban_manager,
    MerchantPromoCodeManager* merchant_promo_code_manager)
    : SingleFieldFormFillRouter(autocomplete_history_manager,
                                iban_manager,
                                merchant_promo_code_manager) {}

MockSingleFieldFormFillRouter::~MockSingleFieldFormFillRouter() = default;

}  // namespace autofill
