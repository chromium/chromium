// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/payments_util.h"

namespace autofill {

CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
    RiskBasedAuthenticationResponse() = default;
CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
    ~RiskBasedAuthenticationResponse() = default;

CreditCardRiskBasedAuthenticator::CreditCardRiskBasedAuthenticator(
    AutofillClient* client)
    : autofill_client_(CHECK_DEREF(client)) {}

CreditCardRiskBasedAuthenticator::~CreditCardRiskBasedAuthenticator() = default;

void CreditCardRiskBasedAuthenticator::Authenticate(
    CreditCard card,
    base::WeakPtr<Requester> requester) {
  // Authenticate should not be called while there is an existing authentication
  // underway.
  DCHECK(!requester_);
  unmask_request_details_ =
      std::make_unique<payments::PaymentsClient::UnmaskRequestDetails>();
  card_ = std::move(card);
  requester_ = requester;

  unmask_request_details_->card = card_;
  if (card_.record_type() == CreditCard::RecordType::kVirtualCard) {
    unmask_request_details_->last_committed_primary_main_frame_origin =
        autofill_client_->GetLastCommittedPrimaryMainFrameURL()
            .DeprecatedGetOriginAsURL();
    if (!autofill_client_->IsOffTheRecord()) {
      unmask_request_details_->merchant_domain_for_footprints =
          autofill_client_->GetLastCommittedPrimaryMainFrameOrigin();
    }
  }
  if (ShouldShowCardMetadata(unmask_request_details_->card)) {
    unmask_request_details_->client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardArtImageAndCardProductName);
  }
  unmask_request_details_->billing_customer_number =
      payments::GetBillingCustomerId(
          autofill_client_->GetPersonalDataManager());

  autofill_client_->GetPaymentsClient()->Prepare();
  autofill_client_->LoadRiskData(
      base::BindOnce(&CreditCardRiskBasedAuthenticator::OnDidGetUnmaskRiskData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardRiskBasedAuthenticator::OnDidGetUnmaskRiskData(
    const std::string& risk_data) {
  unmask_request_details_->risk_data = risk_data;
  autofill_client_->GetPaymentsClient()->UnmaskCard(
      *unmask_request_details_,
      base::BindOnce(
          &CreditCardRiskBasedAuthenticator::OnUnmaskResponseReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardRiskBasedAuthenticator::OnUnmaskResponseReceived(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskResponseDetails& response_details) {
  if (!requester_) {
    Reset();
    return;
  }
  if (unmask_request_details_->card.record_type() ==
      CreditCard::RecordType::kVirtualCard) {
    requester_->OnVirtualCardRiskBasedAuthenticationResponseReceived(
        result, response_details);
    Reset();
    return;
  }

  RiskBasedAuthenticationResponse response;
  if (result == AutofillClient::PaymentsRpcResult::kSuccess) {
    response.did_succeed = true;
    if (!response_details.real_pan.empty()) {
      // The Payments server indicates a green path with real pan returned.
      card_.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
      card_.set_record_type(CreditCard::RecordType::kFullServerCard);
      response.card = card_;
    } else {
      // The Payments server indicates a yellow path with necessary fields
      // returned for further authentication.
      response.fido_request_options =
          std::move(response_details.fido_request_options);
      response.card_unmask_challenge_options =
          response_details.card_unmask_challenge_options;
      response.context_token = response_details.context_token;
    }
  } else {
    // We received an error when attempting to unmask the card.
    response.did_succeed = false;
    CHECK(card_.record_type() == CreditCard::RecordType::kMaskedServerCard);
    response.error_dialog_context.type =
        result == AutofillClient::PaymentsRpcResult::kNetworkError
            ? AutofillErrorDialogType::
                  kMaskedServerCardRiskBasedUnmaskingNetworkError
            : AutofillErrorDialogType::
                  kMaskedServerCardRiskBasedUnmaskingPermanentError;

    // TODO(crbug.com/1470933): Log the error metrics.
  }

  if (requester_) {
    requester_->OnRiskBasedAuthenticationResponseReceived(response);
  }
  Reset();
}

void CreditCardRiskBasedAuthenticator::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  autofill_client_->GetPaymentsClient()->CancelRequest();
  unmask_request_details_.reset();
  requester_.reset();
}

}  // namespace autofill
