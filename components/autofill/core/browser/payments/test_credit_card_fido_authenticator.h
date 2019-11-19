// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_FIDO_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace autofill {

// Test class for CreditCardFIDOAuthenticator.
class TestCreditCardFIDOAuthenticator : public CreditCardFIDOAuthenticator {
 public:
  explicit TestCreditCardFIDOAuthenticator(AutofillDriver* driver,
                                           AutofillClient* client);
  ~TestCreditCardFIDOAuthenticator() override;

  // CreditCardFIDOAuthenticator:
  void GetAssertion(
      PublicKeyCredentialRequestOptionsPtr request_options) override;
  void MakeCredential(
      PublicKeyCredentialCreationOptionsPtr creation_options) override;

  // Invokes fido_authenticator->OnDidGetAssertion().
  static void GetAssertion(CreditCardFIDOAuthenticator* fido_authenticator,
                           bool did_succeed);

  // Invokes fido_authenticator->OnDidMakeCredential().
  static void MakeCredential(CreditCardFIDOAuthenticator* fido_authenticator,
                             bool did_succeed);

  // Getter methods to query Request Options.
  std::vector<uint8_t> GetCredentialId();
  std::vector<uint8_t> GetChallenge();
  std::string GetRelyingPartyId();

  void SetUserVerifiable(bool is_user_verifiable) {
    is_user_verifiable_ = is_user_verifiable;
  }

  // CreditCardFIDOAuthenticator:
  void IsUserVerifiable(base::OnceCallback<void(bool)> callback) override;

 private:
  friend class AutofillManagerTest;
  friend class CreditCardAccessManagerTest;

  PublicKeyCredentialRequestOptionsPtr request_options_;
  PublicKeyCredentialCreationOptionsPtr creation_options_;
  bool is_user_verifiable_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCreditCardFIDOAuthenticator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
