// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"

#include <utility>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace autofill {

TestCreditCardFIDOAuthenticator::TestCreditCardFIDOAuthenticator(
    AutofillDriver* driver,
    AutofillClient* client)
    : CreditCardFIDOAuthenticator(driver, client) {}

TestCreditCardFIDOAuthenticator::~TestCreditCardFIDOAuthenticator() {}

void TestCreditCardFIDOAuthenticator::GetAssertion(
    PublicKeyCredentialRequestOptionsPtr request_options) {
  request_options_ = request_options->Clone();
  CreditCardFIDOAuthenticator::GetAssertion(std::move(request_options));
}

void TestCreditCardFIDOAuthenticator::MakeCredential(
    PublicKeyCredentialCreationOptionsPtr creation_options) {
  creation_options_ = creation_options->Clone();
  CreditCardFIDOAuthenticator::MakeCredential(std::move(creation_options));
}

// static
void TestCreditCardFIDOAuthenticator::GetAssertion(
    CreditCardFIDOAuthenticator* fido_authenticator,
    bool did_succeed) {
  if (did_succeed) {
    GetAssertionAuthenticatorResponsePtr response =
        GetAssertionAuthenticatorResponse::New();
    response->info = blink::mojom::CommonCredentialInfo::New();
    fido_authenticator->OnDidGetAssertion(AuthenticatorStatus::SUCCESS,
                                          std::move(response));
  } else {
    fido_authenticator->OnDidGetAssertion(
        AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  }
}

// static
void TestCreditCardFIDOAuthenticator::MakeCredential(
    CreditCardFIDOAuthenticator* fido_authenticator,
    bool did_succeed) {
  if (did_succeed) {
    MakeCredentialAuthenticatorResponsePtr response =
        MakeCredentialAuthenticatorResponse::New();
    response->info = blink::mojom::CommonCredentialInfo::New();
    fido_authenticator->OnDidMakeCredential(AuthenticatorStatus::SUCCESS,
                                            std::move(response));
  } else {
    fido_authenticator->OnDidMakeCredential(
        AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  }
}

std::vector<uint8_t> TestCreditCardFIDOAuthenticator::GetCredentialId() {
  DCHECK(!request_options_->allow_credentials.empty());
  return request_options_->allow_credentials.front().id();
}

std::vector<uint8_t> TestCreditCardFIDOAuthenticator::GetChallenge() {
  if (request_options_) {
    return request_options_->challenge;
  } else {
    DCHECK(creation_options_);
    return creation_options_->challenge;
  }
}

std::string TestCreditCardFIDOAuthenticator::GetRelyingPartyId() {
  if (request_options_) {
    return request_options_->relying_party_id;
  } else {
    DCHECK(creation_options_);
    return creation_options_->relying_party.id;
  }
}

void TestCreditCardFIDOAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  return std::move(callback).Run(is_user_verifiable_);
}

}  // namespace autofill
