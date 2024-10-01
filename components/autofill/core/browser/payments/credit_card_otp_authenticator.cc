// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {
namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

}  // namespace

CreditCardOtpAuthenticator::OtpAuthenticationResponse::
    OtpAuthenticationResponse() = default;
CreditCardOtpAuthenticator::OtpAuthenticationResponse::
    ~OtpAuthenticationResponse() = default;

CreditCardOtpAuthenticator::CreditCardOtpAuthenticator(AutofillClient* client)
    : autofill_client_(CHECK_DEREF(client)) {}

CreditCardOtpAuthenticator::~CreditCardOtpAuthenticator() = default;

void CreditCardOtpAuthenticator::OnUnmaskPromptAccepted(
    const std::u16string& otp) {
  otp_ = otp;

  unmask_request_ = std::make_unique<
      payments::PaymentsNetworkInterface::UnmaskRequestDetails>();
  unmask_request_->card = *card_;
  unmask_request_->billing_customer_number = billing_customer_number_;
  unmask_request_->context_token = context_token_;
  unmask_request_->otp = otp_;
  unmask_request_->selected_challenge_option = selected_challenge_option_;

  // Add appropriate ClientBehaviorConstants to the request based on the
  // user experience.
  if (ShouldShowCardMetadata(*card_)) {
    unmask_request_->client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardArtImageAndCardProductName);
  }
  if (DidDisplayBenefitForCard(*card_, *autofill_client_,
                               autofill_client_->GetPersonalDataManager()
                                   ->payments_data_manager())) {
    unmask_request_->client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardBenefits);
  }

  if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
    std::optional<GURL> last_committed_primary_main_frame_origin;
    if (autofill_client_->GetLastCommittedPrimaryMainFrameURL().is_valid()) {
      last_committed_primary_main_frame_origin =
          autofill_client_->GetLastCommittedPrimaryMainFrameURL()
              .DeprecatedGetOriginAsURL();
    }
    unmask_request_->last_committed_primary_main_frame_origin =
        last_committed_primary_main_frame_origin;
  }

  // Populating risk data and showing OTP dialog may occur asynchronously.
  // If |risk_data_| has already been loaded, send the unmask card request.
  // Otherwise, continue to wait and let OnDidGetUnmaskRiskData handle it.
  if (!risk_data_.empty()) {
    SendUnmaskCardRequest();
  }
}

void CreditCardOtpAuthenticator::OnUnmaskPromptClosed(bool user_closed_dialog) {
  // This function will be invoked when the prompt closes, no matter if it is
  // due to success or cancellation by users. If the |user_closed_dialog|
  // is false, it means |this| has been reset and logging has completed. We
  // should return early in this case.
  if (!user_closed_dialog)
    return;

  if (requester_) {
    requester_->OnOtpAuthenticationComplete(
        OtpAuthenticationResponse().with_result(
            OtpAuthenticationResponse::Result::kFlowCancelled));
  }

  autofill_metrics::LogOtpAuthResult(
      autofill_metrics::OtpAuthEvent::kFlowCancelled,
      selected_challenge_option_.type);
  Reset();
}

void CreditCardOtpAuthenticator::OnNewOtpRequested() {
  new_otp_requested_ = true;
  if (!selected_challenge_option_request_ongoing_)
    SendSelectChallengeOptionRequest();
}

void CreditCardOtpAuthenticator::OnChallengeOptionSelected(
    const CreditCard* card,
    const CardUnmaskChallengeOption& selected_challenge_option,
    base::WeakPtr<Requester> requester,
    const std::string& context_token,
    int64_t billing_customer_number) {
  if (!card) {
    requester->OnOtpAuthenticationComplete(
        OtpAuthenticationResponse().with_result(
            OtpAuthenticationResponse::Result::kGenericError));
    Reset();
    return;
  }

  // Currently only virtual cards are supported for OTP authentication.
  // If non-virtual cards are allowed for OTP unmasking in the future,
  // |OnDidSelectChallengeOption()| and |OnDidGetRealPan()| should allow for a
  // generic error dialog.
  CHECK_EQ(card->record_type(), CreditCard::RecordType::kVirtualCard);
  CHECK(selected_challenge_option.type ==
            CardUnmaskChallengeOptionType::kSmsOtp ||
        selected_challenge_option.type ==
            CardUnmaskChallengeOptionType::kEmailOtp);
  CHECK(!context_token.empty());
  // Store info for this session. These info will be shared for multiple
  // payments requests. Only |context_token_| will be changed during this
  // session.
  card_ = card;
  selected_challenge_option_ = selected_challenge_option;
  requester_ = requester;
  context_token_ = context_token;
  billing_customer_number_ = billing_customer_number;

  autofill_metrics::LogOtpAuthAttempt(selected_challenge_option_.type);

  // Asynchronously prepare `payments_network_interface`. This is only needed
  // once per session.
  auto* payments_network_interface =
      autofill_client_->GetPaymentsAutofillClient()
          ->GetPaymentsNetworkInterface();
  CHECK(payments_network_interface);
  payments_network_interface->Prepare();

  // Send user selected challenge option to server.
  SendSelectChallengeOptionRequest();
}

void CreditCardOtpAuthenticator::SendSelectChallengeOptionRequest() {
  selected_challenge_option_request_ongoing_ = true;
  // Prepare SelectChallengeOption request.
  select_challenge_option_request_ =
      std::make_unique<payments::PaymentsNetworkInterface::
                           SelectChallengeOptionRequestDetails>();
  select_challenge_option_request_->selected_challenge_option =
      selected_challenge_option_;
  select_challenge_option_request_->billing_customer_number =
      billing_customer_number_;
  select_challenge_option_request_->context_token = context_token_;

  select_challenge_option_request_timestamp_ = base::TimeTicks::Now();

  // Send SelectChallengeOption request to server, the callback is
  // |OnDidSelectChallengeOption|.
  autofill_client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->SelectChallengeOption(
          *select_challenge_option_request_,
          base::BindOnce(
              &CreditCardOtpAuthenticator::OnDidSelectChallengeOption,
              weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardOtpAuthenticator::OnDidSelectChallengeOption(
    PaymentsRpcResult result,
    const std::string& context_token) {
  selected_challenge_option_request_ongoing_ = false;

  if (select_challenge_option_request_timestamp_.has_value()) {
    autofill_metrics::LogOtpAuthSelectChallengeOptionRequestLatency(
        base::TimeTicks::Now() - *select_challenge_option_request_timestamp_,
        selected_challenge_option_.type);
  }

  bool server_success = result == PaymentsRpcResult::kSuccess;
  // Dismiss the pending authentication selection dialog if it is visible so
  // that other dialogs can be shown.
  autofill_client_->GetPaymentsAutofillClient()
      ->DismissUnmaskAuthenticatorSelectionDialog(server_success);
  if (server_success) {
    CHECK(!context_token.empty());
    // Update the |context_token_| with the new one.
    context_token_ = context_token;

    if (!new_otp_requested_) {
      // Display the OTP dialog only if the dialog is not shown yet.
      ShowOtpDialog();
    }
    new_otp_requested_ = false;
    return;
  }

  // If the OTP input dialog is visible, also dismiss it. The two dialogs will
  // not be shown at the same time but either one of them can be visible when
  // this function is invoked.
  autofill_client_->GetPaymentsAutofillClient()->OnUnmaskOtpVerificationResult(
      OtpUnmaskResult::kPermanentFailure);
  // Show the virtual card permanent error dialog if server explicitly returned
  // vcn permanent error, show temporary error dialog for the rest failure cases
  // since currently only virtual card is supported.
  autofill_client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
      AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
          /*is_permanent_error=*/result ==
          PaymentsRpcResult::kVcnRetrievalPermanentFailure));
  if (requester_) {
    OtpAuthenticationResponse response;
    if (result == PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
        result == PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
      autofill_metrics::LogOtpAuthResult(
          autofill_metrics::OtpAuthEvent::
              kSelectedChallengeOptionVirtualCardRetrievalError,
          selected_challenge_option_.type);
      response.result =
          OtpAuthenticationResponse::Result::kVirtualCardRetrievalError;
    } else {
      autofill_metrics::LogOtpAuthResult(
          autofill_metrics::OtpAuthEvent::kSelectedChallengeOptionGenericError,
          selected_challenge_option_.type);
      response.result = OtpAuthenticationResponse::Result::kAuthenticationError;
    }
    requester_->OnOtpAuthenticationComplete(response);
  }

  Reset();
}

void CreditCardOtpAuthenticator::ShowOtpDialog() {
  // Before showing OTP dialog, let's load required risk data if it's not
  // prepared. Risk data is only required for unmask request. Not required for
  // select challenge option request.
  if (risk_data_.empty()) {
    autofill_client_->GetPaymentsAutofillClient()->LoadRiskData(
        base::BindOnce(&CreditCardOtpAuthenticator::OnDidGetUnmaskRiskData,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  autofill_client_->GetPaymentsAutofillClient()->ShowCardUnmaskOtpInputDialog(
      selected_challenge_option_, weak_ptr_factory_.GetWeakPtr());
}

void CreditCardOtpAuthenticator::OnDidGetUnmaskRiskData(
    const std::string& risk_data) {
  risk_data_ = risk_data;
  // Populating risk data and showing OTP dialog may occur asynchronously.
  // If the dialog has already been accepted (otp is provided), send the unmask
  // card request. Otherwise, continue to wait for the user to accept the OTP
  // dialog.
  if (!otp_.empty()) {
    SendUnmaskCardRequest();
  }
}

void CreditCardOtpAuthenticator::SendUnmaskCardRequest() {
  unmask_request_->risk_data = risk_data_;
  unmask_card_request_timestamp_ = base::TimeTicks::Now();
  autofill_client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->UnmaskCard(*unmask_request_,
                   base::BindOnce(&CreditCardOtpAuthenticator::OnDidGetRealPan,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardOtpAuthenticator::OnDidGetRealPan(
    PaymentsRpcResult result,
    const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
        response_details) {
  if (unmask_card_request_timestamp_.has_value()) {
    autofill_metrics::LogOtpAuthUnmaskCardRequestLatency(
        base::TimeTicks::Now() - *unmask_card_request_timestamp_,
        selected_challenge_option_.type);
  }

  if (result == PaymentsRpcResult::kSuccess) {
    if (response_details.card_type !=
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard) {
      // Currently we offer OTP authentication only for virtual cards.
      NOTREACHED_IN_MIGRATION();
      if (requester_) {
        requester_->OnOtpAuthenticationComplete(
            OtpAuthenticationResponse().with_result(
                OtpAuthenticationResponse::Result::kGenericError));
      }

      Reset();
      return;
    }
    // If |flow_status| is present, this intermediate status allows the user to
    // stay in the current session to finish the unmasking with certain user
    // actions rather than ending the flow.
    if (!response_details.flow_status.empty()) {
      CHECK(!response_details.context_token.empty());
      // Update the |context_token_| with the new one.
      context_token_ = response_details.context_token;

      // Invoke autofill_client to update the OTP dialog with the flow status,
      // e.g. OTP mismatch or expired.
      if (response_details.flow_status.find("INCORRECT_OTP") !=
          std::string::npos) {
        autofill_client_->GetPaymentsAutofillClient()
            ->OnUnmaskOtpVerificationResult(OtpUnmaskResult::kOtpMismatch);
        autofill_metrics::LogOtpAuthRetriableError(
            autofill_metrics::OtpAuthEvent::kOtpMismatch,
            selected_challenge_option_.type);
      } else {
        CHECK(response_details.flow_status.find("EXPIRED_OTP") !=
              std::string::npos);
        autofill_client_->GetPaymentsAutofillClient()
            ->OnUnmaskOtpVerificationResult(OtpUnmaskResult::kOtpExpired);
        autofill_metrics::LogOtpAuthRetriableError(
            autofill_metrics::OtpAuthEvent::kOtpExpired,
            selected_challenge_option_.type);
      }
      return;
    }

    // The following prerequisites should be ensured in the
    // PaymentsNetworkInterface.
    CHECK(!response_details.real_pan.empty());
    CHECK(!response_details.dcvv.empty());
    CHECK(!response_details.expiration_month.empty());
    CHECK(!response_details.expiration_year.empty());

    unmask_request_->card.SetNumber(
        base::UTF8ToUTF16(response_details.real_pan));
    unmask_request_->card.set_record_type(CreditCard::RecordType::kVirtualCard);
    unmask_request_->card.SetExpirationMonthFromString(
        base::UTF8ToUTF16(response_details.expiration_month),
        /*app_locale=*/std::string());
    unmask_request_->card.SetExpirationYearFromString(
        base::UTF8ToUTF16(response_details.expiration_year));

    if (requester_) {
      auto response = OtpAuthenticationResponse().with_result(
          OtpAuthenticationResponse::Result::kSuccess);
      response.card = &(unmask_request_->card);
      response.cvc = base::UTF8ToUTF16(response_details.dcvv);
      requester_->OnOtpAuthenticationComplete(response);
    }

    autofill_client_->GetPaymentsAutofillClient()
        ->OnUnmaskOtpVerificationResult(OtpUnmaskResult::kSuccess);

    autofill_metrics::LogOtpAuthResult(autofill_metrics::OtpAuthEvent::kSuccess,
                                       selected_challenge_option_.type);
    Reset();
    return;
  }

  // Show the virtual card permanent error dialog if server explicitly returned
  // vcn permanent error, show temporary error dialog for the remaining failure
  // cases since currently only virtual card is supported.
  if (requester_) {
    OtpAuthenticationResponse response;
    if (result == PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
        result == PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
      response.result =
          OtpAuthenticationResponse::Result::kVirtualCardRetrievalError;
      autofill_metrics::LogOtpAuthResult(
          autofill_metrics::OtpAuthEvent::kUnmaskCardVirtualCardRetrievalError,
          selected_challenge_option_.type);
    } else {
      response.result = OtpAuthenticationResponse::Result::kAuthenticationError;
      autofill_metrics::LogOtpAuthResult(
          autofill_metrics::OtpAuthEvent::kUnmaskCardAuthError,
          selected_challenge_option_.type);
    }
    requester_->OnOtpAuthenticationComplete(response);
  }

  autofill_client_->GetPaymentsAutofillClient()->OnUnmaskOtpVerificationResult(
      OtpUnmaskResult::kPermanentFailure);

  // If the server returned error dialog fields to be displayed, we prefer them
  // since they will be more detailed to the specific error that occurred.
  if (response_details.autofill_error_dialog_context) {
    autofill_client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
        *response_details.autofill_error_dialog_context);
  } else {
    autofill_client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/result ==
            PaymentsRpcResult::kVcnRetrievalPermanentFailure));
  }
  Reset();
}

void CreditCardOtpAuthenticator::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  autofill_client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->CancelRequest();
  card_ = nullptr;
  selected_challenge_option_ = CardUnmaskChallengeOption();
  otp_ = std::u16string();
  context_token_ = std::string();
  risk_data_ = std::string();
  billing_customer_number_ = 0;
  selected_challenge_option_request_ongoing_ = false;
  select_challenge_option_request_.reset();
  unmask_request_.reset();
  select_challenge_option_request_timestamp_ = std::nullopt;
  unmask_card_request_timestamp_ = std::nullopt;
  new_otp_requested_ = false;
}

}  // namespace autofill
