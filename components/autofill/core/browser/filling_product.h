// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_

#include "components/autofill/core/browser/ui/popup_item_ids.h"

namespace autofill {

// Denotes the entity that is responsible for an Autofill behavior.
enum class FillingProduct {
  // kNone is used for the suggestions that do not identify any Autofill entity.
  kNone,
  kAddress,
  kCreditCard,
  kMerchantPromoCode,
  kIban,
  kAutocomplete,
  kPasswordManager,
  kCompose,
  kPlusAddresses,
  kMaxValue = kPlusAddresses
};

FillingProduct GetFillingProductFromPopupItemId(PopupItemId popup_item_id);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_
