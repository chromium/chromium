// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_TEST_CREDIT_CARD_OTP_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_TEST_CREDIT_CARD_OTP_AUTHENTICATOR_H_

#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"

#include <string>

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

namespace autofill {

class AutofillClient;

// Test class for CreditCardOtpAuthenticator.
class TestCreditCardOtpAuthenticator : public CreditCardOtpAuthenticator {
 public:
  explicit TestCreditCardOtpAuthenticator(AutofillClient* client);
  TestCreditCardOtpAuthenticator(const TestCreditCardOtpAuthenticator&) =
      delete;
  TestCreditCardOtpAuthenticator& operator=(
      const TestCreditCardOtpAuthenticator&) = delete;
  ~TestCreditCardOtpAuthenticator() override;

  // CreditCardOtpAuthenticator:
  void OnChallengeOptionSelected(
      const CreditCard* card,
      const CardUnmaskChallengeOption& selected_challenge_option,
      base::WeakPtr<Requester> requester,
      const std::string& context_token,
      int64_t billing_customer_number) override;

  // Reset all testing related states.
  void Reset() override;

  const CreditCard& card() { return card_; }
  const CardUnmaskChallengeOption& selected_challenge_option() {
    return selected_challenge_option_;
  }
  const std::string& context_token() { return context_token_; }
  bool on_challenge_option_selected_invoked() {
    return on_challenge_option_selected_invoked_;
  }

 private:
  CreditCard card_;
  CardUnmaskChallengeOption selected_challenge_option_;
  std::string context_token_;
  bool on_challenge_option_selected_invoked_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_TEST_CREDIT_CARD_OTP_AUTHENTICATOR_H_
