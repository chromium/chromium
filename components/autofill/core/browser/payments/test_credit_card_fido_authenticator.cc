// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"

#include <string>
#include <utility>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace autofill {

TestCreditCardFidoAuthenticator::TestCreditCardFidoAuthenticator(
    AutofillDriver* driver,
    AutofillClient* client)
    : CreditCardFidoAuthenticator(driver, client) {}

TestCreditCardFidoAuthenticator::~TestCreditCardFidoAuthenticator() = default;

void TestCreditCardFidoAuthenticator::Authenticate(
    CreditCard card,
    base::WeakPtr<Requester> requester,
    base::Value::Dict request_options,
    std::optional<std::string> context_token) {
  authenticate_invoked_ = true;
  card_ = std::move(card);
  context_token_ = context_token;
  CreditCardFidoAuthenticator::Authenticate(
      *card_, requester, std::move(request_options), context_token);
}

void TestCreditCardFidoAuthenticator::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr request_options) {
  request_options_ = request_options->Clone();
  CreditCardFidoAuthenticator::GetAssertion(std::move(request_options));
}

void TestCreditCardFidoAuthenticator::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr creation_options) {
  creation_options_ = creation_options->Clone();
  CreditCardFidoAuthenticator::MakeCredential(std::move(creation_options));
}

void TestCreditCardFidoAuthenticator::OptOut() {
  opt_out_called_ = true;
  CreditCardFidoAuthenticator::OptOut();
}

// static
void TestCreditCardFidoAuthenticator::GetAssertion(
    CreditCardFidoAuthenticator* fido_authenticator,
    bool did_succeed) {
  if (did_succeed) {
    blink::mojom::GetAssertionAuthenticatorResponsePtr response =
        blink::mojom::GetAssertionAuthenticatorResponse::New();
    response->info = blink::mojom::CommonCredentialInfo::New();
    response->extensions =
        blink::mojom::AuthenticationExtensionsClientOutputs::New();
    fido_authenticator->OnDidGetAssertion(
        blink::mojom::AuthenticatorStatus::SUCCESS, std::move(response),
        /*dom_exception_details=*/nullptr);
  } else {
    fido_authenticator->OnDidGetAssertion(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
        /*dom_exception_details=*/nullptr);
  }
}

// static
void TestCreditCardFidoAuthenticator::MakeCredential(
    CreditCardFidoAuthenticator* fido_authenticator,
    bool did_succeed) {
  if (did_succeed) {
    blink::mojom::MakeCredentialAuthenticatorResponsePtr response =
        blink::mojom::MakeCredentialAuthenticatorResponse::New();
    response->info = blink::mojom::CommonCredentialInfo::New();
    fido_authenticator->OnDidMakeCredential(
        blink::mojom::AuthenticatorStatus::SUCCESS, std::move(response),
        /*dom_exception_details=*/nullptr);
  } else {
    fido_authenticator->OnDidMakeCredential(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
        /*dom_exception_details=*/nullptr);
  }
}

std::vector<uint8_t> TestCreditCardFidoAuthenticator::GetCredentialId() {
  DCHECK(!request_options_->allow_credentials.empty());
  return request_options_->allow_credentials.front().id;
}

std::vector<uint8_t> TestCreditCardFidoAuthenticator::GetChallenge() {
  if (request_options_) {
    return request_options_->challenge;
  } else {
    DCHECK(creation_options_);
    return creation_options_->challenge;
  }
}

std::string TestCreditCardFidoAuthenticator::GetRelyingPartyId() {
  if (request_options_) {
    return request_options_->relying_party_id;
  } else {
    DCHECK(creation_options_);
    return creation_options_->relying_party.id;
  }
}

void TestCreditCardFidoAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  return std::move(callback).Run(is_user_verifiable_);
}

bool TestCreditCardFidoAuthenticator::IsUserOptedIn() {
  if (is_user_opted_in_.has_value())
    return is_user_opted_in_.value();

  return CreditCardFidoAuthenticator::IsUserOptedIn();
}

void TestCreditCardFidoAuthenticator::Reset() {
  is_user_verifiable_ = false;
  is_user_opted_in_ = std::nullopt;
  opt_out_called_ = false;
  authenticate_invoked_ = false;
  card_ = CreditCard();
}

}  // namespace autofill
