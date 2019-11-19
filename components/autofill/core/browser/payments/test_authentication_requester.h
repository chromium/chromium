// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"

#if !defined(OS_IOS)
#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"
#endif

namespace autofill {

// Test class for requesting authentication from CreditCardCVCAuthenticator or
// CreditCardFIDOAuthenticator.
#if defined(OS_IOS)
class TestAuthenticationRequester
    : public CreditCardCVCAuthenticator::Requester {
#else
class TestAuthenticationRequester
    : public CreditCardCVCAuthenticator::Requester,
      public CreditCardFIDOAuthenticator::Requester {
#endif
 public:
  TestAuthenticationRequester();
  ~TestAuthenticationRequester() override;

  // CreditCardCVCAuthenticator::Requester:
  void OnCVCAuthenticationComplete(
      const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response)
      override;

#if !defined(OS_IOS)
  // CreditCardFIDOAuthenticator::Requester:
  void OnFIDOAuthenticationComplete(bool did_succeed,
                                    const CreditCard* card = nullptr) override;

  void IsUserVerifiableCallback(bool is_user_verifiable);
#endif

  base::WeakPtr<TestAuthenticationRequester> GetWeakPtr();

  base::Optional<bool> is_user_verifiable() { return is_user_verifiable_; }

  bool did_succeed() { return did_succeed_; }

  base::string16 number() { return number_; }

 private:
  // Set when CreditCardFIDOAuthenticator invokes IsUserVerifiableCallback().
  base::Optional<bool> is_user_verifiable_;

  // Is set to true if authentication was successful.
  bool did_succeed_ = false;

  // The card number returned from On*AuthenticationComplete().
  base::string16 number_;

  base::WeakPtrFactory<TestAuthenticationRequester> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_
