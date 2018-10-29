// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/full_card_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {
namespace payments {

FullCardRequest::FullCardRequest(RiskDataLoader* risk_data_loader,
                                 payments::PaymentsClient* payments_client,
                                 PersonalDataManager* personal_data_manager)
    : FullCardRequest(risk_data_loader,
                      payments_client,
                      personal_data_manager,
                      base::TimeTicks()) {}

FullCardRequest::FullCardRequest(RiskDataLoader* risk_data_loader,
                                 payments::PaymentsClient* payments_client,
                                 PersonalDataManager* personal_data_manager,
                                 base::TimeTicks form_parsed_timestamp)
    : risk_data_loader_(risk_data_loader),
      payments_client_(payments_client),
      personal_data_manager_(personal_data_manager),
      result_delegate_(nullptr),
      ui_delegate_(nullptr),
      should_unmask_card_(false),
      form_parsed_timestamp_(form_parsed_timestamp),
      weak_ptr_factory_(this) {
  DCHECK(risk_data_loader_);
  DCHECK(payments_client_);
  DCHECK(personal_data_manager_);
}

FullCardRequest::~FullCardRequest() {}

void FullCardRequest::GetFullCard(const CreditCard& card,
                                  AutofillClient::UnmaskCardReason reason,
                                  base::WeakPtr<ResultDelegate> result_delegate,
                                  base::WeakPtr<UIDelegate> ui_delegate) {
  DCHECK(result_delegate);
  DCHECK(ui_delegate);

  // Only one request can be active at a time. If the member variable
  // |result_delegate_| is already set, then immediately reject the new request
  // through the method parameter |result_delegate_|.
  if (result_delegate_) {
    result_delegate_->OnFullCardRequestFailed();
    return;
  }

  result_delegate_ = result_delegate;
  ui_delegate_ = ui_delegate;
  request_.reset(new payments::PaymentsClient::UnmaskRequestDetails);
  request_->card = card;
  should_unmask_card_ = card.record_type() == CreditCard::MASKED_SERVER_CARD ||
                        (card.record_type() == CreditCard::FULL_SERVER_CARD &&
                         card.ShouldUpdateExpiration(AutofillClock::Now()));
  if (should_unmask_card_) {
    payments_client_->Prepare();
    request_->billing_customer_number = GetBillingCustomerId(
        personal_data_manager_, payments_client_->GetPrefService(),
        /*should_log_validity=*/true);
  }

  ui_delegate_->ShowUnmaskPrompt(request_->card, reason,
                                 weak_ptr_factory_.GetWeakPtr());

  if (should_unmask_card_) {
    risk_data_loader_->LoadRiskData(
        base::Bind(&FullCardRequest::OnDidGetUnmaskRiskData,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

bool FullCardRequest::IsGettingFullCard() const {
  return !!request_;
}

void FullCardRequest::OnUnmaskResponse(const UnmaskResponse& response) {
  if (!response.exp_month.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_MONTH, response.exp_month);

  if (!response.exp_year.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, response.exp_year);

  if (request_->card.record_type() == CreditCard::LOCAL_CARD &&
      !request_->card.guid().empty() &&
      (!response.exp_month.empty() || !response.exp_year.empty())) {
    personal_data_manager_->UpdateCreditCard(request_->card);
  }

  if (!should_unmask_card_) {
    if (result_delegate_)
      result_delegate_->OnFullCardRequestSucceeded(*this, request_->card,
                                                   response.cvc);
    if (ui_delegate_)
      ui_delegate_->OnUnmaskVerificationResult(AutofillClient::SUCCESS);
    Reset();

    return;
  }

  request_->user_response = response;
  if (!request_->risk_data.empty())
    SendUnmaskCardRequest();
}

void FullCardRequest::OnUnmaskPromptClosed() {
  if (result_delegate_)
    result_delegate_->OnFullCardRequestFailed();

  Reset();
}

void FullCardRequest::OnDidGetUnmaskRiskData(const std::string& risk_data) {
  request_->risk_data = risk_data;
  if (!request_->user_response.cvc.empty())
    SendUnmaskCardRequest();
}

void FullCardRequest::SendUnmaskCardRequest() {
  real_pan_request_timestamp_ = AutofillClock::Now();
  payments_client_->UnmaskCard(*request_,
                               base::BindOnce(&FullCardRequest::OnDidGetRealPan,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void FullCardRequest::OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                                      const std::string& real_pan) {
  AutofillMetrics::LogRealPanDuration(
      AutofillClock::Now() - real_pan_request_timestamp_, result);

  if (ui_delegate_)
    ui_delegate_->OnUnmaskVerificationResult(result);

  switch (result) {
    // Wait for user retry.
    case AutofillClient::TRY_AGAIN_FAILURE:
      break;

    // Neither PERMANENT_FAILURE nor NETWORK_ERROR allow retry.
    case AutofillClient::PERMANENT_FAILURE:
    // Intentional fall through.
    case AutofillClient::NETWORK_ERROR: {
      if (result_delegate_)
        result_delegate_->OnFullCardRequestFailed();
      Reset();
      break;
    }

    case AutofillClient::SUCCESS: {
      DCHECK(!real_pan.empty());
      request_->card.set_record_type(CreditCard::FULL_SERVER_CARD);
      request_->card.SetNumber(base::UTF8ToUTF16(real_pan));
      request_->card.SetServerStatus(CreditCard::OK);

      if (request_->user_response.should_store_pan)
        personal_data_manager_->UpdateServerCreditCard(request_->card);

      if (result_delegate_)
        result_delegate_->OnFullCardRequestSucceeded(
            *this, request_->card, request_->user_response.cvc);
      Reset();
      break;
    }

    case AutofillClient::NONE:
      NOTREACHED();
      break;
  }
}

void FullCardRequest::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  payments_client_->CancelRequest();
  result_delegate_ = nullptr;
  ui_delegate_ = nullptr;
  request_.reset();
  should_unmask_card_ = false;
}

}  // namespace payments
}  // namespace autofill
