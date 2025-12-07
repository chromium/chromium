// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/common/autofill_payments_features.h"

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
  card_unmask_challenge_options = other.card_unmask_challenge_options;
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
  unmask_request_details_ = std::make_unique<payments::UnmaskRequestDetails>();
  card_ = std::move(card);
  requester_ = requester;
  unmask_request_details_->card = card_;
  if (card_.record_type() == CreditCard::RecordType::kVirtualCard ||
      card_.card_info_retrieval_enrollment_state() ==
          CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled) {
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
                               autofill_client_.get())) {
    unmask_request_details_->client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardBenefits);
  }

  unmask_request_details_->billing_customer_number =
      payments::GetBillingCustomerId(
          autofill_client_->GetPersonalDataManager().payments_data_manager());

  GetPaymentsNetworkInterface().Prepare();
  autofill_client_->GetPaymentsAutofillClient()->LoadRiskData(
      base::BindOnce(&CreditCardRiskBasedAuthenticator::OnDidGetUnmaskRiskData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardRiskBasedAuthenticator::OnDidGetUnmaskRiskData(
    const std::string& risk_data) {
  unmask_request_details_->risk_data = risk_data;
  autofill_metrics::LogRiskBasedAuthAttempt(card_.record_type());
  unmask_card_request_timestamp_ = base::TimeTicks::Now();
  GetPaymentsNetworkInterface().UnmaskCard(
      *unmask_request_details_,
      base::BindOnce(
          &CreditCardRiskBasedAuthenticator::OnUnmaskResponseReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardRiskBasedAuthenticator::OnUnmaskResponseReceived(
    PaymentsRpcResult result,
    const payments::UnmaskResponseDetails& response_details) {
  if (unmask_card_request_timestamp_.has_value()) {
    autofill_metrics::LogRiskBasedAuthLatency(
        base::TimeTicks::Now() - unmask_card_request_timestamp_.value(),
        card_.record_type());
  }

  if (!requester_) {
    Reset();
    return;
  }

  RiskBasedAuthenticationResponse response;
  CreditCard::RecordType record_type =
      unmask_request_details_->card.record_type();
  CHECK(record_type == CreditCard::RecordType::kMaskedServerCard ||
        record_type == CreditCard::RecordType::kVirtualCard);
  if (result == PaymentsRpcResult::kSuccess) {
    if (!response_details.real_pan.empty()) {
      // The Payments server indicates no further authentication is required.
      response.result =
          RiskBasedAuthenticationResponse::Result::kNoAuthenticationRequired;
      card_.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
      if (record_type == CreditCard::RecordType::kMaskedServerCard) {
        card_.set_record_type(CreditCard::RecordType::kFullServerCard);
      }

      // Use server provided expiration date if the unmasked card is a virtual
      // card. As virtual cards have different expiration dates.
      if (record_type == CreditCard::RecordType::kVirtualCard) {
        card_.SetExpirationMonthFromString(
            base::UTF8ToUTF16(response_details.expiration_month),
            /*app_locale=*/std::string());
        card_.SetExpirationYearFromString(
            base::UTF8ToUTF16(response_details.expiration_year));
      }
      // Use server provided CVC if applicable.
      if (ShouldUseServerProvidedCvc(unmask_request_details_->card)) {
        CHECK(!response_details.dcvv.empty());
        card_.set_cvc(base::UTF8ToUTF16(response_details.dcvv));
      }

      response.card = card_;

      autofill_metrics::LogRiskBasedAuthResult(
          record_type,
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
      response.card_unmask_challenge_options =
          std::move(response_details.card_unmask_challenge_options);

      autofill_metrics::LogRiskBasedAuthResult(
          record_type,
          autofill_metrics::RiskBasedAuthEvent::kAuthenticationRequired);
    }
  } else {
    // We received an error when attempting to unmask the card.
    response.result = RiskBasedAuthenticationResponse::Result::kError;
    if (record_type == CreditCard::RecordType::kMaskedServerCard) {
      if (result == PaymentsRpcResult::kNetworkError) {
        response.error_dialog_context.type = AutofillErrorDialogType::
            kMaskedServerCardRiskBasedUnmaskingNetworkError;
      } else if (card_.card_info_retrieval_enrollment_state() ==
                 CreditCard::CardInfoRetrievalEnrollmentState::
                     kRetrievalEnrolled) {
        response.error_dialog_context = AutofillErrorDialogContext::
            WithCardInfoRetrievalPermanentOrTemporaryError(
                /*is_permanent_error=*/result ==
                PaymentsRpcResult::kPermanentFailure);
      } else {
        response.error_dialog_context.type = AutofillErrorDialogType::
            kMaskedServerCardRiskBasedUnmaskingPermanentError;
      }
    } else if (record_type == CreditCard::RecordType::kVirtualCard) {
      if (result == PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
          result == PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
        response.result =
            RiskBasedAuthenticationResponse::Result::kVirtualCardRetrievalError;
      }
      response.error_dialog_context =
          response_details.autofill_error_dialog_context.value_or(
              response.error_dialog_context.AutofillErrorDialogContext::
                  WithVirtualCardPermanentOrTemporaryError(
                      /*is_permanent_error=*/result ==
                      PaymentsRpcResult::kVcnRetrievalPermanentFailure));
    }

    autofill_metrics::LogRiskBasedAuthResult(
        record_type, autofill_metrics::RiskBasedAuthEvent::kUnexpectedError);
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
  if (requester_) {
    requester_->OnRiskBasedAuthenticationResponseReceived(
        RiskBasedAuthenticationResponse().with_result(
            RiskBasedAuthenticationResponse::Result::kAuthenticationCancelled));
  }
  Reset();
}

bool CreditCardRiskBasedAuthenticator::ShouldUseServerProvidedCvc(
    const CreditCard card) {
  return card.record_type() == CreditCard::RecordType::kVirtualCard ||
         card.card_info_retrieval_enrollment_state() ==
             CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled;
}

void CreditCardRiskBasedAuthenticator::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetPaymentsNetworkInterface().CancelRequest();
  unmask_request_details_.reset();
  requester_.reset();
  unmask_card_request_timestamp_.reset();
}

}  // namespace autofill
