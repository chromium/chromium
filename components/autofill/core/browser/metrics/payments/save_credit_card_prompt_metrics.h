// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_CREDIT_CARD_PROMPT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_CREDIT_CARD_PROMPT_METRICS_H_

#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

enum class SaveCreditCardPromptResult {
  // User accepted save.
  kAccepted = 0,
  // User declined to save card.
  kDenied = 1,
  // User did not interact with the flow.
  kIgnored = 2,
  // User interacted but then ignored, without explicitly accepting or
  // cancelling.
  kInteractedAndIgnored = 3,
  kMaxValue = kInteractedAndIgnored,
};

void LogSaveCreditCardPromptResult(
    SaveCreditCardPromptResult event,
    bool is_upload,
    AutofillClient::SaveCreditCardOptions options);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_CREDIT_CARD_PROMPT_METRICS_H_
