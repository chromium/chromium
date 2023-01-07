// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/test_credit_card_otp_authenticator.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"

namespace autofill {

TestCreditCardOtpAuthenticator::TestCreditCardOtpAuthenticator(
    AutofillClient* client)
    : CreditCardOtpAuthenticator(client) {}

TestCreditCardOtpAuthenticator::~TestCreditCardOtpAuthenticator() = default;

void TestCreditCardOtpAuthenticator::OnChallengeOptionSelected(
    const CreditCard* card,
    const CardUnmaskChallengeOption& selected_challenge_option,
    base::WeakPtr<Requester> requester,
    const std::string& context_token,
    int64_t billing_customer_number) {
  on_challenge_option_selected_invoked_ = true;
  card_ = *card;
  selected_challenge_option_ = selected_challenge_option;
  context_token_ = context_token;
}

void TestCreditCardOtpAuthenticator::Reset() {
  on_challenge_option_selected_invoked_ = false;
  card_ = CreditCard();
  selected_challenge_option_ = CardUnmaskChallengeOption();
  context_token_ = std::string();
}

}  // namespace autofill
