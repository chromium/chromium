// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_authentication_requester.h"

#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

namespace autofill {

TestAuthenticationRequester::TestAuthenticationRequester() = default;

TestAuthenticationRequester::~TestAuthenticationRequester() = default;

base::WeakPtr<TestAuthenticationRequester>
TestAuthenticationRequester::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TestAuthenticationRequester::OnCvcAuthenticationComplete(
    const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response) {
  did_succeed_ = response.did_succeed;
  if (*did_succeed_) {
    DCHECK(response.card);
    number_ = response.card->number();
    record_type_ = response.card->record_type();
  }
}

#if BUILDFLAG(IS_ANDROID)
bool TestAuthenticationRequester::ShouldOfferFidoAuth() const {
  return false;
}

bool TestAuthenticationRequester::UserOptedInToFidoFromSettingsPageOnMobile()
    const {
  return false;
}
#endif

#if !BUILDFLAG(IS_IOS)
void TestAuthenticationRequester::OnFIDOAuthenticationComplete(
    const CreditCardFidoAuthenticator::FidoAuthenticationResponse& response) {
  did_succeed_ = response.did_succeed;
  if (*did_succeed_) {
    DCHECK(response.card);
    number_ = response.card->number();
    record_type_ = response.card->record_type();
  }
  failure_type_ = response.failure_type;
}

void TestAuthenticationRequester::OnFidoAuthorizationComplete(
    bool did_succeed) {
  did_succeed_ = did_succeed;
}

void TestAuthenticationRequester::IsUserVerifiableCallback(
    bool is_user_verifiable) {
  is_user_verifiable_ = is_user_verifiable;
}
#endif

void TestAuthenticationRequester::OnOtpAuthenticationComplete(
    const CreditCardOtpAuthenticator::OtpAuthenticationResponse& response) {
  did_succeed_ =
      response.result ==
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess;
  if (*did_succeed_) {
    DCHECK(response.card);
    number_ = response.card->number();
    record_type_ = response.card->record_type();
  }
}

void TestAuthenticationRequester::OnRiskBasedAuthenticationResponseReceived(
    const CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse&
        response) {
  did_succeed_ =
      (response.result ==
       CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
           Result::kNoAuthenticationRequired) ||
      (response.result ==
       CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
           Result::kAuthenticationRequired);
  risk_based_authentication_response_ = response;
}

}  // namespace autofill
