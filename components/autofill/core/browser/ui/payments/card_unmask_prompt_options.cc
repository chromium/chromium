// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"

namespace autofill {

CardUnmaskPromptOptions::CardUnmaskPromptOptions() = default;

CardUnmaskPromptOptions::CardUnmaskPromptOptions(
    const std::optional<CardUnmaskChallengeOption>& challenge_option,
    payments::PaymentsAutofillClient::UnmaskCardReason reason)
    : challenge_option(challenge_option), reason(reason) {}

CardUnmaskPromptOptions::CardUnmaskPromptOptions(
    const CardUnmaskPromptOptions&) = default;

CardUnmaskPromptOptions::~CardUnmaskPromptOptions() = default;

}  // namespace autofill
