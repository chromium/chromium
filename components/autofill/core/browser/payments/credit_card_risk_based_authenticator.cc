// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {
namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

}  // namespace

CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
    RiskBasedAuthenticationResponse() = default;
CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse&
CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::operator=(
    const CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse&
        other) {
  result = other.result;
  error_dialog_context = other.error_dialog_context;
  card = other.card;
  if (other.fido_request_options.empty()) {
    fido_request_options.clear();
  } else {
    fido_request_options = other.fido_request_options.Clone();
  }
  context_token = other.context_token;
  return *this;
}
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
  unmask_request_details_ = std::make_unique<
      payments::PaymentsNetworkInterface::UnmaskRequestDetails>();
  card_ = std::move(card);
  requester_ = requester;

  unmask_request_details_->card = card_;
  if (card_.record_type() == CreditCard::RecordType::kVirtualCard) {
    unmask_request_details_->last_committed_primary_main_frame_origin =
        autofill_client_->GetLastCommittedPrimaryMainFrameURL()
            .DeprecatedGetOriginAsURL();
  }

  // Add appropriate ClientBehaviorConstants to the request based on the
  // user experience.
  if (ShouldShowCardMetadata(unmask_request_details_->card)) {
    unmask_request_details_->client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardArtImageAndCardProductName);
  }
  if (DidDisplayBenefitForCard(unmask_request_details_->card,
                               autofill_client_.get(),
                               autofill_client_->GetPersonalDataManager()
                                   ->payments_data_manager())) {
    unmask_request_details_->client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardBenefits);
  }

  unmask_request_details_->billing_customer_number =
      payments::GetBillingCustomerId(
          &autofill_client_->GetPersonalDataManager()->payments_data_manager());

  autofill_client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->Prepare();
  autofill_client_->GetPaymentsAutofillClient()->LoadRiskData(
      base::BindOnce(&CreditCardRiskBasedAuthenticator::OnDidGetUnmaskRiskData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardRiskBasedAuthenticator::OnDidGetUnmaskRiskData(
    const std::string& risk_data) {
  unmask_request_details_->risk_data = risk_data;
  autofill_metrics::LogRiskBasedAuthAttempt(card_.record_type());
  unmask_card_request_timestamp_ = base::TimeTicks::Now();
  autofill_client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->UnmaskCard(
          *unmask_request_details_,
          base::BindOnce(
              &CreditCardRiskBasedAuthenticator::OnUnmaskResponseReceived,
              weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardRiskBasedAuthenticator::OnUnmaskResponseReceived(
    PaymentsRpcResult result,
    const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
        response_details) {
  if (unmask_card_request_timestamp_.has_value()) {
    autofill_metrics::LogRiskBasedAuthLatency(
        base::TimeTicks::Now() - unmask_card_request_timestamp_.value(),
        card_.record_type());
  }

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
  if (result == PaymentsRpcResult::kSuccess) {
    if (!response_details.real_pan.empty()) {
      // The Payments server indicates no further authentication is required.
      response.result =
          RiskBasedAuthenticationResponse::Result::kNoAuthenticationRequired;
      card_.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
      card_.set_record_type(CreditCard::RecordType::kFullServerCard);
      response.card = card_;

      autofill_metrics::LogRiskBasedAuthResult(
          CreditCard::RecordType::kMaskedServerCard,
          autofill_metrics::RiskBasedAuthEvent::kNoAuthenticationRequired);
    } else {
      // The Payments server indicates further authentication is required.
      response.result =
          RiskBasedAuthenticationResponse::Result::kAuthenticationRequired;
      if (!response_details.fido_request_options.empty()) {
        response.fido_request_options =
            response_details.fido_request_options.Clone();
      }
      response.context_token = response_details.context_token;

      autofill_metrics::LogRiskBasedAuthResult(
          CreditCard::RecordType::kMaskedServerCard,
          autofill_metrics::RiskBasedAuthEvent::kAuthenticationRequired);
    }
  } else {
    // We received an error when attempting to unmask the card.
    response.result = RiskBasedAuthenticationResponse::Result::kError;
    CHECK(card_.record_type() == CreditCard::RecordType::kMaskedServerCard);
    response.error_dialog_context.type =
        result == PaymentsRpcResult::kNetworkError
            ? AutofillErrorDialogType::
                  kMaskedServerCardRiskBasedUnmaskingNetworkError
            : AutofillErrorDialogType::
                  kMaskedServerCardRiskBasedUnmaskingPermanentError;

    autofill_metrics::LogRiskBasedAuthResult(
        CreditCard::RecordType::kMaskedServerCard,
        autofill_metrics::RiskBasedAuthEvent::kUnexpectedError);
  }

  if (requester_) {
    requester_->OnRiskBasedAuthenticationResponseReceived(response);
  }
  Reset();
}

void CreditCardRiskBasedAuthenticator::OnUnmaskCancelled() {
  autofill_metrics::LogRiskBasedAuthResult(
      CreditCard::RecordType::kMaskedServerCard,
      autofill_metrics::RiskBasedAuthEvent::kAuthenticationCancelled);

  requester_->OnRiskBasedAuthenticationResponseReceived(
      RiskBasedAuthenticationResponse().with_result(
          RiskBasedAuthenticationResponse::Result::kAuthenticationCancelled));
  Reset();
}

void CreditCardRiskBasedAuthenticator::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  autofill_client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->CancelRequest();
  unmask_request_details_.reset();
  requester_.reset();
  unmask_card_request_timestamp_.reset();
}

}  // namespace autofill
