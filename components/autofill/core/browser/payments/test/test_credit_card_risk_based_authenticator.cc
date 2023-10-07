// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/test_credit_card_risk_based_authenticator.h"

#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

TestCreditCardRiskBasedAuthenticator::TestCreditCardRiskBasedAuthenticator(
    AutofillClient* client)
    : CreditCardRiskBasedAuthenticator(client) {}

TestCreditCardRiskBasedAuthenticator::~TestCreditCardRiskBasedAuthenticator() =
    default;

void TestCreditCardRiskBasedAuthenticator::Authenticate(
    CreditCard card,
    base::WeakPtr<Requester> requester) {
  authenticate_invoked_ = true;
  card_ = std::move(card);
}

void TestCreditCardRiskBasedAuthenticator::Reset() {
  authenticate_invoked_ = false;
  card_ = CreditCard();
}

}  // namespace autofill
