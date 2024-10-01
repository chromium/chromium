// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

CreditCardCvcAuthenticator::CvcAuthenticationResponse::
    CvcAuthenticationResponse() = default;
CreditCardCvcAuthenticator::CvcAuthenticationResponse::
    ~CvcAuthenticationResponse() = default;

CreditCardCvcAuthenticator::CreditCardCvcAuthenticator(AutofillClient* client)
    : client_(client) {}

CreditCardCvcAuthenticator::~CreditCardCvcAuthenticator() = default;

void CreditCardCvcAuthenticator::Authenticate(
    const CreditCard& card,
    base::WeakPtr<Requester> requester,
    PersonalDataManager* personal_data_manager,
    std::optional<std::string> context_token,
    std::optional<CardUnmaskChallengeOption> selected_challenge_option) {
  requester_ = requester;

  full_card_request_ = std::make_unique<payments::FullCardRequest>(
      client_,
      client_->GetPaymentsAutofillClient()->GetPaymentsNetworkInterface(),
      personal_data_manager);

  CreditCard::RecordType card_record_type = card.record_type();
  autofill_metrics::LogCvcAuthAttempt(card_record_type);
  if (card_record_type == CreditCard::RecordType::kVirtualCard) {
    // `context_token` and `challenge_option` are required for
    // `FullCardRequest::GetFullVirtualCardViaCVC()`, so DCHECK that they are
    // present. The caller of Authenticate() should ensure to always set these
    // variables for the virtual card case.
    DCHECK(context_token);
    DCHECK(selected_challenge_option);
    DCHECK_EQ(selected_challenge_option->type,
              CardUnmaskChallengeOptionType::kCvc);

    const GURL& last_committed_primary_main_frame_origin =
        client_->GetLastCommittedPrimaryMainFrameURL()
            .DeprecatedGetOriginAsURL();

    // We need a valid last committed primary main frame origin for virtual card
    // CVC unmasking. Thus, if it is not a valid last committed primary main
    // frame origin, end the card unmasking and treat it as a transient failure.
    if (!last_committed_primary_main_frame_origin.is_valid()) {
      return OnFullCardRequestFailed(
          card.record_type(), payments::FullCardRequest::FailureType::
                                  VIRTUAL_CARD_RETRIEVAL_TRANSIENT_FAILURE);
    }

    return full_card_request_->GetFullVirtualCardViaCVC(
        card, payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill,
        weak_ptr_factory_.GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(),
        last_committed_primary_main_frame_origin, *context_token,
        *selected_challenge_option);
  }

  full_card_request_->GetFullCard(
      card, payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill,
      weak_ptr_factory_.GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(),
      context_token);
}

void CreditCardCvcAuthenticator::OnFullCardRequestSucceeded(
    const payments::FullCardRequest& full_card_request,
    const CreditCard& card,
    const std::u16string& cvc) {
  autofill_metrics::LogCvcAuthResult(card.record_type(),
                                     autofill_metrics::CvcAuthEvent::kSuccess);

  if (!requester_)
    return;

  payments::PaymentsNetworkInterface::UnmaskResponseDetails response =
      full_card_request.unmask_response_details();
  requester_->OnCvcAuthenticationComplete(
      CvcAuthenticationResponse()
          .with_did_succeed(true)
          .with_card(&card)
          .with_cvc(cvc)
          .with_request_options(std::move(response.fido_request_options))
          .with_card_authorization_token(response.card_authorization_token));
}

void CreditCardCvcAuthenticator::OnFullCardRequestFailed(
    CreditCard::RecordType card_type,
    payments::FullCardRequest::FailureType failure_type) {
  autofill_metrics::CvcAuthEvent event =
      autofill_metrics::CvcAuthEvent::kUnknown;
  switch (failure_type) {
    case payments::FullCardRequest::FailureType::PROMPT_CLOSED:
      event = autofill_metrics::CvcAuthEvent::kFlowCancelled;
      break;
    case payments::FullCardRequest::FailureType::VERIFICATION_DECLINED:
      event = autofill_metrics::CvcAuthEvent::kUnmaskCardAuthError;
      break;
    case payments::FullCardRequest::FailureType::
        VIRTUAL_CARD_RETRIEVAL_TRANSIENT_FAILURE:
    case payments::FullCardRequest::FailureType::
        VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE:
      event =
          autofill_metrics::CvcAuthEvent::kUnmaskCardVirtualCardRetrievalError;
      break;
    case payments::FullCardRequest::FailureType::GENERIC_FAILURE:
      event = autofill_metrics::CvcAuthEvent::kGenericError;
      break;
    case payments::FullCardRequest::FailureType::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      event = autofill_metrics::CvcAuthEvent::kUnknown;
      break;
  }
  autofill_metrics::LogCvcAuthResult(card_type, event);

  if (!requester_)
    return;

  requester_->OnCvcAuthenticationComplete(
      CvcAuthenticationResponse().with_did_succeed(false));
}

void CreditCardCvcAuthenticator::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  client_->GetPaymentsAutofillClient()->ShowUnmaskPrompt(
      card, card_unmask_prompt_options, delegate);
}

void CreditCardCvcAuthenticator::OnUnmaskVerificationResult(
    payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  client_->GetPaymentsAutofillClient()->OnUnmaskVerificationResult(result);
}

#if BUILDFLAG(IS_ANDROID)
bool CreditCardCvcAuthenticator::ShouldOfferFidoAuth() const {
  return requester_ && requester_->ShouldOfferFidoAuth();
}

bool CreditCardCvcAuthenticator::UserOptedInToFidoFromSettingsPageOnMobile()
    const {
  return requester_ && requester_->UserOptedInToFidoFromSettingsPageOnMobile();
}
#endif

payments::FullCardRequest* CreditCardCvcAuthenticator::GetFullCardRequest() {
  // TODO(crbug.com/40622637): iOS and Android clients should use
  // CreditCardAccessManager to retrieve cards from payments instead of calling
  // this function directly.
  if (!full_card_request_) {
    full_card_request_ = std::make_unique<payments::FullCardRequest>(
        client_,
        client_->GetPaymentsAutofillClient()->GetPaymentsNetworkInterface(),
        client_->GetPersonalDataManager());
  }
  return full_card_request_.get();
}

base::WeakPtr<payments::FullCardRequest::UIDelegate>
CreditCardCvcAuthenticator::GetAsFullCardRequestUIDelegate() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
