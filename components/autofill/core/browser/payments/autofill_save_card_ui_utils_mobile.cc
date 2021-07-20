// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_ui_utils_mobile.h"

#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"

namespace autofill {

int GetSaveCardIconId(bool is_google_pay_branding_enabled) {
  return is_google_pay_branding_enabled ? IDR_AUTOFILL_GOOGLE_PAY
                                        : IDR_INFOBAR_AUTOFILL_CC;
}

void UpdateAutofillAcceptSaveCreditCardPromptState(PrefService* pref_service,
                                                   bool accepted) {
  pref_service->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      accepted ? prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED
               : prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED);
}

}  // namespace autofill
