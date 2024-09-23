// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_OPTIONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_OPTIONS_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

// Holds additional parameters needed to call ShowDialog in the Card Unmask
// Prompt Controller.
struct CardUnmaskPromptOptions {
  CardUnmaskPromptOptions();
  CardUnmaskPromptOptions(
      const std::optional<CardUnmaskChallengeOption>& challenge_option,
      payments::PaymentsAutofillClient::UnmaskCardReason reason);
  CardUnmaskPromptOptions(const CardUnmaskPromptOptions&);
  ~CardUnmaskPromptOptions();

  // Present only for a virtual card. Contains more information regarding the
  // challenge option the user is presented with. In the
  // CardUnmaskPromptController, only a CardUnmaskChallengeOptionType::kCvc
  // challenge option is supported.
  std::optional<CardUnmaskChallengeOption> challenge_option;

  // The origin of the unmask request.
  payments::PaymentsAutofillClient::UnmaskCardReason reason;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_OPTIONS_H_
