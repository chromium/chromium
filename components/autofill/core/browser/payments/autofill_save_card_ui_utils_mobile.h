// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_UI_UTILS_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_UI_UTILS_MOBILE_H_

#include "components/prefs/pref_service.h"

namespace autofill {

// Returns the resource id of expected ui icon.
int GetSaveCardIconId(bool is_google_pay_branding_enabled);

// Update the local pref of whether users have agreed to save card.
// |pref_service| is a weak reference to read & write
// |kAutofillAcceptSaveCreditCardPromptState|.
void UpdateAutofillAcceptSaveCreditCardPromptState(PrefService* pref_service,
                                                   bool accepted);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_UI_UTILS_MOBILE_H_
