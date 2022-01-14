// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"

namespace autofill {

CreditCardCVCAuthenticator::CVCAuthenticationResponse::
    CVCAuthenticationResponse() {}
CreditCardCVCAuthenticator::CVCAuthenticationResponse::
    ~CVCAuthenticationResponse() {}

CreditCardCVCAuthenticator::CreditCardCVCAuthenticator(AutofillClient* client)
    : client_(client) {}

CreditCardCVCAuthenticator::~CreditCardCVCAuthenticator() {}

void CreditCardCVCAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    PersonalDataManager* personal_data_manager) {
  requester_ = requester;
  if (!card) {
    return OnFullCardRequestFailed(
        payments::FullCardRequest::FailureType::GENERIC_FAILURE);
  }
  full_card_request_ = std::make_unique<payments::FullCardRequest>(
      client_, client_->GetPaymentsClient(), personal_data_manager);

  absl::optional<GURL> last_committed_url_origin;
  if (card->record_type() == CreditCard::VIRTUAL_CARD &&
      client_->GetLastCommittedURL().is_valid()) {
    last_committed_url_origin =
        client_->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  }

  full_card_request_->GetFullCard(
      *card, AutofillClient::UnmaskCardReason::kAutofill,
      weak_ptr_factory_.GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(),
      last_committed_url_origin);
}

void CreditCardCVCAuthenticator::OnFullCardRequestSucceeded(
    const payments::FullCardRequest& full_card_request,
    const CreditCard& card,
    const std::u16string& cvc) {
  payments::PaymentsClient::UnmaskResponseDetails response =
      full_card_request.unmask_response_details();
  requester_->OnCVCAuthenticationComplete(
      CVCAuthenticationResponse()
          .with_did_succeed(true)
          .with_card(&card)
          .with_cvc(cvc)
          .with_creation_options(std::move(response.fido_creation_options))
          .with_request_options(std::move(response.fido_request_options))
          .with_card_authorization_token(response.card_authorization_token));
}

void CreditCardCVCAuthenticator::OnFullCardRequestFailed(
    payments::FullCardRequest::FailureType failure_type) {
  requester_->OnCVCAuthenticationComplete(
      CVCAuthenticationResponse().with_did_succeed(false));
}

void CreditCardCVCAuthenticator::ShowUnmaskPrompt(
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  client_->ShowUnmaskPrompt(card, reason, delegate);
}

void CreditCardCVCAuthenticator::OnUnmaskVerificationResult(
    AutofillClient::PaymentsRpcResult result) {
  client_->OnUnmaskVerificationResult(result);
}

#if BUILDFLAG(IS_ANDROID)
bool CreditCardCVCAuthenticator::ShouldOfferFidoAuth() const {
  return requester_ && requester_->ShouldOfferFidoAuth();
}

bool CreditCardCVCAuthenticator::UserOptedInToFidoFromSettingsPageOnMobile()
    const {
  return requester_ && requester_->UserOptedInToFidoFromSettingsPageOnMobile();
}
#endif

payments::FullCardRequest* CreditCardCVCAuthenticator::GetFullCardRequest() {
  // TODO(crbug.com/951669): iOS and Android clients should use
  // CreditCardAccessManager to retrieve cards from payments instead of calling
  // this function directly.
  if (!full_card_request_) {
    full_card_request_ = std::make_unique<payments::FullCardRequest>(
        client_, client_->GetPaymentsClient(),
        client_->GetPersonalDataManager());
  }
  return full_card_request_.get();
}

base::WeakPtr<payments::FullCardRequest::UIDelegate>
CreditCardCVCAuthenticator::GetAsFullCardRequestUIDelegate() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
