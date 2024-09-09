// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_FIDO_AUTHENTICATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

namespace autofill {

// Test class for CreditCardFidoAuthenticator.
class TestCreditCardFidoAuthenticator : public CreditCardFidoAuthenticator {
 public:
  explicit TestCreditCardFidoAuthenticator(AutofillDriver* driver,
                                           AutofillClient* client);

  TestCreditCardFidoAuthenticator(const TestCreditCardFidoAuthenticator&) =
      delete;
  TestCreditCardFidoAuthenticator& operator=(
      const TestCreditCardFidoAuthenticator&) = delete;

  ~TestCreditCardFidoAuthenticator() override;

  // CreditCardFidoAuthenticator:
  void Authenticate(CreditCard card,
                    base::WeakPtr<Requester> requester,
                    base::Value::Dict request_options,
                    std::optional<std::string> context_token) override;
  void IsUserVerifiable(base::OnceCallback<void(bool)> callback) override;
  bool IsUserOptedIn() override;
  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr
                        request_options) override;
  void MakeCredential(blink::mojom::PublicKeyCredentialCreationOptionsPtr
                          creation_options) override;
  void OptOut() override;

  // Invokes fido_authenticator->OnDidGetAssertion().
  static void GetAssertion(CreditCardFidoAuthenticator* fido_authenticator,
                           bool did_succeed);

  // Invokes fido_authenticator->OnDidMakeCredential().
  static void MakeCredential(CreditCardFidoAuthenticator* fido_authenticator,
                             bool did_succeed);

  // Getter methods to query Request Options.
  std::vector<uint8_t> GetCredentialId();
  std::vector<uint8_t> GetChallenge();
  std::string GetRelyingPartyId();

  void SetUserVerifiable(bool is_user_verifiable) {
    is_user_verifiable_ = is_user_verifiable;
  }

  void set_is_user_opted_in(bool is_user_opted_in) {
    is_user_opted_in_ = is_user_opted_in;
  }

  bool IsOptOutCalled() { return opt_out_called_; }
  bool authenticate_invoked() { return authenticate_invoked_; }
  const CreditCard& card() { return *card_; }
  const std::optional<std::string>& context_token() { return context_token_; }

  // Resets all the testing related states.
  void Reset();

 private:
  friend class BrowserAutofillManagerTest;
  friend class CreditCardAccessManagerTestBase;

  blink::mojom::PublicKeyCredentialRequestOptionsPtr request_options_;
  blink::mojom::PublicKeyCredentialCreationOptionsPtr creation_options_;
  bool is_user_verifiable_ = false;
  std::optional<bool> is_user_opted_in_;
  bool opt_out_called_ = false;
  bool authenticate_invoked_ = false;
  std::optional<CreditCard> card_;
  std::optional<std::string> context_token_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
