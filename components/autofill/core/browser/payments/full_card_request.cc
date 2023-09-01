// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/full_card_request.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"

namespace autofill {
namespace payments {

FullCardRequest::FullCardRequest(RiskDataLoader* risk_data_loader,
                                 payments::PaymentsClient* payments_client,
                                 PersonalDataManager* personal_data_manager)
    : risk_data_loader_(risk_data_loader),
      payments_client_(payments_client),
      personal_data_manager_(personal_data_manager),
      result_delegate_(nullptr),
      ui_delegate_(nullptr),
      should_unmask_card_(false) {
  DCHECK(risk_data_loader_);
  DCHECK(payments_client_);
  DCHECK(personal_data_manager_);
}

FullCardRequest::~FullCardRequest() = default;

void FullCardRequest::GetFullCard(const CreditCard& card,
                                  AutofillClient::UnmaskCardReason reason,
                                  base::WeakPtr<ResultDelegate> result_delegate,
                                  base::WeakPtr<UIDelegate> ui_delegate) {
  DCHECK(ui_delegate);
  GetFullCardImpl(card, reason, result_delegate, ui_delegate,
                  /*fido_assertion_info=*/absl::nullopt,
                  /*last_committed_primary_main_frame_origin=*/absl::nullopt,
                  /*context_token=*/absl::nullopt,
                  /*selected_challenge_option=*/absl::nullopt);
}

void FullCardRequest::GetFullVirtualCardViaCVC(
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<ResultDelegate> result_delegate,
    base::WeakPtr<UIDelegate> ui_delegate,
    const GURL& last_committed_primary_main_frame_origin,
    const std::string& vcn_context_token,
    const CardUnmaskChallengeOption& selected_challenge_option) {
  DCHECK(ui_delegate);
  DCHECK(last_committed_primary_main_frame_origin.is_valid());
  DCHECK(!vcn_context_token.empty());
  DCHECK(selected_challenge_option.type == CardUnmaskChallengeOptionType::kCvc);
  GetFullCardImpl(card, reason, result_delegate, ui_delegate,
                  /*fido_assertion_info=*/absl::nullopt,
                  last_committed_primary_main_frame_origin, vcn_context_token,
                  selected_challenge_option);
}

void FullCardRequest::GetFullCardViaFIDO(
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<ResultDelegate> result_delegate,
    base::Value::Dict fido_assertion_info,
    absl::optional<GURL> last_committed_primary_main_frame_origin,
    absl::optional<std::string> context_token) {
  GetFullCardImpl(
      card, reason, result_delegate, nullptr, std::move(fido_assertion_info),
      std::move(last_committed_primary_main_frame_origin),
      std::move(context_token), /*selected_challenge_option=*/absl::nullopt);
}

void FullCardRequest::GetFullCardImpl(
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<ResultDelegate> result_delegate,
    base::WeakPtr<UIDelegate> ui_delegate,
    absl::optional<base::Value::Dict> fido_assertion_info,
    absl::optional<GURL> last_committed_primary_main_frame_origin,
    absl::optional<std::string> context_token,
    absl::optional<CardUnmaskChallengeOption> selected_challenge_option) {
  // Retrieval of card information should happen via CVC auth or FIDO, but not
  // both. Use |ui_delegate|'s existence as evidence of doing CVC auth and
  // |fido_assertion_info| as evidence of doing FIDO auth.
  DCHECK_NE(fido_assertion_info.has_value(), !!ui_delegate);
  DCHECK(result_delegate);

  CreditCard::RecordType card_type = card.record_type();

  // Only one request can be active at a time. If the member variable
  // |result_delegate_| is already set, then immediately reject the new request
  // through the method parameter |result_delegate_|.
  if (result_delegate_) {
    result_delegate_->OnFullCardRequestFailed(card_type,
                                              FailureType::GENERIC_FAILURE);
    return;
  }
  result_delegate_ = result_delegate;
  ui_delegate_ = ui_delegate;

  // If unmasking is for a virtual card and
  // |last_committed_primary_main_frame_origin| is empty, end the request as
  // failure and reset.
  if (card.record_type() == CreditCard::RecordType::kVirtualCard &&
      !last_committed_primary_main_frame_origin.has_value()) {
    NOTREACHED();
    if (ui_delegate_) {
      ui_delegate_->OnUnmaskVerificationResult(
          AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure);
    }

    if (result_delegate_) {
      result_delegate_->OnFullCardRequestFailed(
          card_type, FailureType::VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE);
    }

    Reset();
    return;
  }

  request_ = std::make_unique<payments::PaymentsClient::UnmaskRequestDetails>();
  request_->card = card;
  request_->last_committed_primary_main_frame_origin =
      last_committed_primary_main_frame_origin;
  if (context_token)
    request_->context_token = *context_token;
  if (selected_challenge_option)
    request_->selected_challenge_option = selected_challenge_option;

  should_unmask_card_ = card.masked() ||
                        (card_type == CreditCard::RecordType::kFullServerCard &&
                         card.ShouldUpdateExpiration()) ||
                        (card_type == CreditCard::RecordType::kVirtualCard);
  if (should_unmask_card_) {
    payments_client_->Prepare();
    request_->billing_customer_number =
        GetBillingCustomerId(personal_data_manager_);
  }

  request_->fido_assertion_info = std::move(fido_assertion_info);

  if (ShouldShowCardMetadata(card)) {
    request_->client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardArtImageAndCardProductName);
  }

  // If there is a UI delegate, then perform a CVC check.
  // Otherwise, continue and use |fido_assertion_info| to unmask.
  if (ui_delegate_) {
    ui_delegate_->ShowUnmaskPrompt(
        request_->card,
        CardUnmaskPromptOptions(selected_challenge_option, reason),
        weak_ptr_factory_.GetWeakPtr());
  }

  if (should_unmask_card_) {
    risk_data_loader_->LoadRiskData(
        base::BindOnce(&FullCardRequest::OnDidGetUnmaskRiskData,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FullCardRequest::OnUnmaskPromptAccepted(
    const UserProvidedUnmaskDetails& user_response) {
  if (!user_response.exp_month.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_MONTH, user_response.exp_month);

  if (!user_response.exp_year.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                              user_response.exp_year);

  if (request_->card.record_type() == CreditCard::RecordType::kLocalCard &&
      !request_->card.guid().empty() &&
      (!user_response.exp_month.empty() || !user_response.exp_year.empty())) {
    personal_data_manager_->UpdateCreditCard(request_->card);
  }

  if (!should_unmask_card_) {
    if (result_delegate_)
      result_delegate_->OnFullCardRequestSucceeded(*this, request_->card,
                                                   user_response.cvc);
    if (ui_delegate_)
      ui_delegate_->OnUnmaskVerificationResult(
          AutofillClient::PaymentsRpcResult::kSuccess);
    Reset();

    return;
  }

  request_->user_response = user_response;
#if BUILDFLAG(IS_ANDROID)
  if (ui_delegate_) {
    // An opt-in request to Payments must be included either if the user chose
    // to opt-in through the CVC prompt or if the UI delegate indicates that the
    // user previously chose to opt-in through the settings page.
    request_->user_response.enable_fido_auth =
        user_response.enable_fido_auth ||
        ui_delegate_->UserOptedInToFidoFromSettingsPageOnMobile();
  }
#endif

  if (!request_->risk_data.empty())
    SendUnmaskCardRequest();
}

void FullCardRequest::OnUnmaskPromptClosed() {
  if (result_delegate_) {
    result_delegate_->OnFullCardRequestFailed(request_->card.record_type(),
                                              FailureType::PROMPT_CLOSED);
  }

  Reset();
}

bool FullCardRequest::ShouldOfferFidoAuth() const {
  // FIDO opt-in is only handled from card unmask on mobile. Desktop platforms
  // provide a separate opt-in bubble.
#if BUILDFLAG(IS_ANDROID)
  return ui_delegate_ && ui_delegate_->ShouldOfferFidoAuth();
#else
  return false;
#endif
}

void FullCardRequest::OnDidGetUnmaskRiskData(const std::string& risk_data) {
  request_->risk_data = risk_data;
  if (!request_->user_response.cvc.empty() ||
      request_->fido_assertion_info.has_value()) {
    SendUnmaskCardRequest();
  }
}

void FullCardRequest::SendUnmaskCardRequest() {
  real_pan_request_timestamp_ = AutofillTickClock::NowTicks();
  payments_client_->UnmaskCard(*request_,
                               base::BindOnce(&FullCardRequest::OnDidGetRealPan,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void FullCardRequest::OnDidGetRealPan(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskResponseDetails& response_details) {
  // If the CVC field is populated, that means the user performed a CVC check.
  // If FIDO AssertionInfo is populated, then the user must have performed FIDO
  // authentication. Exactly one of these fields must be populated.
  DCHECK_NE(!request_->user_response.cvc.empty(),
            request_->fido_assertion_info.has_value());

  // Update the existing context token to the most up-to-date one received from
  // the payments server. The context token in the response details can be an
  // empty string for regular server cards. It will be populated in situations
  // where the server needs to connect multiple steps of the unmasking flow
  // together, such as in the case of virtual cards.
  request_->context_token = response_details.context_token;

  AutofillClient::PaymentsRpcCardType card_type = response_details.card_type;
  if (!request_->user_response.cvc.empty()) {
    AutofillMetrics::LogRealPanDuration(
        AutofillTickClock::NowTicks() - real_pan_request_timestamp_, result,
        card_type);
  } else if (request_->fido_assertion_info.has_value()) {
    autofill_metrics::LogCardUnmaskDurationAfterWebauthn(
        AutofillTickClock::NowTicks() - real_pan_request_timestamp_, result,
        card_type);
  }

  if (ui_delegate_)
    ui_delegate_->OnUnmaskVerificationResult(result);

  switch (result) {
    // Wait for user retry.
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure: {
      autofill_metrics::LogCvcAuthRetryableError(
          request_->card.record_type(),
          request_->card.ShouldUpdateExpiration()
              ? autofill_metrics::CvcAuthEvent::kTemporaryErrorExpiredCard
              : autofill_metrics::CvcAuthEvent::kTemporaryErrorCvcMismatch);
      break;
    }
    // Neither PERMANENT_FAILURE, NETWORK_ERROR nor VCN retrieval errors allow
    // retry.
    case AutofillClient::PaymentsRpcResult::kPermanentFailure: {
      if (result_delegate_) {
        result_delegate_->OnFullCardRequestFailed(
            request_->card.record_type(), FailureType::VERIFICATION_DECLINED);
      }
      Reset();
      break;
    }
    case AutofillClient::PaymentsRpcResult::kNetworkError: {
      if (result_delegate_) {
        result_delegate_->OnFullCardRequestFailed(request_->card.record_type(),
                                                  FailureType::GENERIC_FAILURE);
      }
      Reset();
      break;
    }
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure: {
      if (result_delegate_) {
        result_delegate_->OnFullCardRequestFailed(
            request_->card.record_type(),
            FailureType::VIRTUAL_CARD_RETRIEVAL_TRANSIENT_FAILURE);
      }
      Reset();
      break;
    }
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure: {
      if (result_delegate_) {
        result_delegate_->OnFullCardRequestFailed(
            request_->card.record_type(),
            FailureType::VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE);
      }
      Reset();
      break;
    }

    case AutofillClient::PaymentsRpcResult::kSuccess: {
      DCHECK(!response_details.real_pan.empty());
      request_->card.SetNumber(base::UTF8ToUTF16(response_details.real_pan));

      if (response_details.card_type ==
          AutofillClient::PaymentsRpcCardType::kVirtualCard) {
        request_->card.set_record_type(CreditCard::RecordType::kVirtualCard);
        request_->card.SetExpirationMonthFromString(
            base::UTF8ToUTF16(response_details.expiration_month),
            /*app_locale=*/std::string());
        request_->card.SetExpirationYearFromString(
            base::UTF8ToUTF16(response_details.expiration_year));
        // `request_->card` will already already have a CVC set as it's the card
        // from the autofill table, so we only need to override from the server
        // response in the virtual card case.
        request_->card.set_cvc(base::UTF8ToUTF16(response_details.dcvv));
      } else if (response_details.card_type ==
                 AutofillClient::PaymentsRpcCardType::kServerCard) {
        request_->card.set_record_type(CreditCard::RecordType::kFullServerCard);
      } else {
        NOTREACHED();
      }

      // TODO(crbug/949269): Once |fido_opt_in| is added to
      // UserProvidedUnmaskDetails, clear out |creation_options| from
      // |response_details_| if |user_response.fido_opt_in| was not set to true
      // to avoid an unwanted registration prompt.
      unmask_response_details_ = response_details;

      const std::u16string cvc = !response_details.dcvv.empty()
                                     ? base::UTF8ToUTF16(response_details.dcvv)
                                     : request_->user_response.cvc;
      if (result_delegate_) {
        result_delegate_->OnFullCardRequestSucceeded(*this, request_->card,
                                                     cvc);
      }
      Reset();
      break;
    }

    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      break;
  }
}

void FullCardRequest::OnFIDOVerificationCancelled() {
  Reset();
}

void FullCardRequest::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  payments_client_->CancelRequest();
  result_delegate_ = nullptr;
  ui_delegate_ = nullptr;
  request_.reset();
  should_unmask_card_ = false;
  unmask_response_details_ = payments::PaymentsClient::UnmaskResponseDetails();
}

}  // namespace payments
}  // namespace autofill
