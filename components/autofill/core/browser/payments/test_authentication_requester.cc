// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_authentication_requester.h"

#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

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
  }
}

void TestAuthenticationRequester::OnRiskBasedAuthenticationResponseReceived(
    const CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse&
        response) {
  risk_based_authentication_response_ = response;
}

void TestAuthenticationRequester::
    OnVirtualCardRiskBasedAuthenticationResponseReceived(
        payments::PaymentsAutofillClient::PaymentsRpcResult result,
        const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
            response_details) {
  did_succeed_ =
      (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
  if (*did_succeed_) {
    response_details_ = response_details;
  }
}

}  // namespace autofill
