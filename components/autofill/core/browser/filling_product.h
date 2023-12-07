// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_

#include "components/autofill/core/browser/ui/popup_item_ids.h"

namespace autofill {

// Denotes the entity that is responsible for an Autofill behavior.
enum class FillingProduct {
  // kNone means that Autofill is not the entity causing a certain behavior.
  kNone = 0,
  kAddressAutofill = 1,
  kPaymentsAutofill = 2,
  kAutocomplete = 3,
  kPasswordManager = 4,
  kCompose = 5,
  kPlusAddresses = 6,
  kMaxValue = kPlusAddresses
};

FillingProduct GetFillingProductFromPopupItemId(PopupItemId popup_item_id);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_
