// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_TEST_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_TEST_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_

#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

namespace autofill {

class TestCreditCardRiskBasedAuthenticator
    : public CreditCardRiskBasedAuthenticator {
 public:
  explicit TestCreditCardRiskBasedAuthenticator(AutofillClient* client);
  TestCreditCardRiskBasedAuthenticator(
      const TestCreditCardRiskBasedAuthenticator&) = delete;
  TestCreditCardRiskBasedAuthenticator& operator=(
      const TestCreditCardRiskBasedAuthenticator&) = delete;
  ~TestCreditCardRiskBasedAuthenticator() override;

  // CreditCardRiskBasedAuthenticator:
  void Authenticate(CreditCard card,
                    base::WeakPtr<Requester> requester) override;

  bool authenticate_invoked() const { return authenticate_invoked_; }
  const CreditCard& card() const { return *card_; }
  int64_t billing_customer_id() const { return billing_customer_id_; }

  // Resets all the testing related states.
  void Reset() override;

 private:
  bool authenticate_invoked_ = false;
  std::optional<CreditCard> card_;
  int64_t billing_customer_id_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_TEST_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_
