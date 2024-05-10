// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments::facilitated {

FacilitatedPaymentsManager::FacilitatedPaymentsManager(
    FacilitatedPaymentsDriver* driver,
    FacilitatedPaymentsClient* client,
    std::unique_ptr<FacilitatedPaymentsApiClient> api_client,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : driver_(*driver),
      client_(*client),
      api_client_(std::move(api_client)),
      optimization_guide_decider_(optimization_guide_decider),
      initiate_payment_request_details_(
          std::make_unique<
              FacilitatedPaymentsInitiatePaymentRequestDetails>()) {
  DCHECK(optimization_guide_decider_);
  RegisterPixAllowlist();
}

FacilitatedPaymentsManager::~FacilitatedPaymentsManager() = default;

void FacilitatedPaymentsManager::Reset() {
  // In tests, when the payment flow is abandoned, do not reset so the final
  // states can be verified.
  if (is_test_) {
    return;
  }
  pix_code_detection_attempt_count_ = 0;
  ukm_source_id_ = 0;
  pix_code_detection_triggering_timer_.Stop();
  initiate_payment_request_details_ =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void FacilitatedPaymentsManager::
    DelayedCheckAllowlistAndTriggerPixCodeDetection(const GURL& url,
                                                    ukm::SourceId ukm_source_id,
                                                    int attempt_number) {
  Reset();
  switch (GetAllowlistCheckResult(url)) {
    case optimization_guide::OptimizationGuideDecision::kTrue: {
      ukm_source_id_ = ukm_source_id;
      initiate_payment_request_details_->merchant_payment_page_url_ = url;
      // The PIX code detection should be triggered after `kPageLoadWaitTime`.
      // Time spent waiting for the allowlist checking infra should be accounted
      // for.
      base::TimeDelta trigger_pix_code_detection_delay =
          std::max(base::Seconds(0),
                   kPageLoadWaitTime - (attempt_number - 1) *
                                           kOptimizationGuideDeciderWaitTime);
      DelayedTriggerPixCodeDetection(trigger_pix_code_detection_delay);
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kUnknown: {
      if (attempt_number >= kMaxAttemptsForAllowlistCheck) {
        break;
      }
      pix_code_detection_triggering_timer_.Start(
          FROM_HERE, kOptimizationGuideDeciderWaitTime,
          base::BindOnce(&FacilitatedPaymentsManager::
                             DelayedCheckAllowlistAndTriggerPixCodeDetection,
                         weak_ptr_factory_.GetWeakPtr(), url, ukm_source_id,
                         attempt_number + 1));
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kFalse:
      break;
  }
}

void FacilitatedPaymentsManager::RegisterPixAllowlist() const {
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST});
}

optimization_guide::OptimizationGuideDecision
FacilitatedPaymentsManager::GetAllowlistCheckResult(const GURL& url) const {
  // Since the optimization guide decider integration corresponding to PIX
  // merchant lists are allowlists for the question "Can this site be
  // optimized?", a match on the allowlist answers the question with "yes".
  // Therefore, `kTrue` indicates that `url` is allowed for running PIX code
  // detection. If the optimization type was not registered in time when we
  // queried it, it will be `kUnknown`.
  return optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST,
      /*optimization_metadata=*/nullptr);
}

void FacilitatedPaymentsManager::DelayedTriggerPixCodeDetection(
    base::TimeDelta delay) {
  pix_code_detection_triggering_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&FacilitatedPaymentsManager::TriggerPixCodeDetection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::TriggerPixCodeDetection() {
  pix_code_detection_attempt_count_++;
  StartPixCodeDetectionLatencyTimer();
  driver_->TriggerPixCodeDetection(
      base::BindOnce(&FacilitatedPaymentsManager::ProcessPixCodeDetectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::ProcessPixCodeDetectionResult(
    mojom::PixCodeDetectionResult result, const std::string& pix_code) {
  // If a PIX code was not found, re-trigger PIX code detection after a short
  // duration to allow async content to load completely.
  if (result == mojom::PixCodeDetectionResult::kPixCodeNotFound &&
      pix_code_detection_attempt_count_ < kMaxAttemptsForPixCodeDetection) {
    DelayedTriggerPixCodeDetection(kRetriggerPixCodeDetectionWaitTime);
    return;
  }
  ukm::builders::FacilitatedPayments_PixCodeDetectionResult(ukm_source_id_)
      .SetResult(static_cast<uint8_t>(result))
      .SetLatencyInMillis(GetPixCodeDetectionLatencyInMillis())
      .SetAttempts(pix_code_detection_attempt_count_)
      .SetDetectionTriggeredOnDomContentLoaded(
          base::FeatureList::IsEnabled(kEnablePixDetectionOnDomContentLoaded))
      .Record(ukm::UkmRecorder::Get());

  // If a valid PIX code is found, and the user has Google wallet linked PIX
  // accounts, verify that the payments API is available, and then show the PIX
  // payment prompt.
  auto* personal_data_manager = client_->GetPersonalDataManager();
  if (!personal_data_manager) {
    Reset();
    return;
  }
  if (result != mojom::PixCodeDetectionResult::kValidPixCodeFound ||
      !personal_data_manager->payments_data_manager().HasMaskedBankAccounts() ||
      !base::FeatureList::IsEnabled(kEnablePixPayments)) {
    Reset();
    return;
  }

  utility_process_validator_.ValidatePixCode(
      pix_code, base::BindOnce(&FacilitatedPaymentsManager::OnPixCodeValidated,
                               weak_ptr_factory_.GetWeakPtr(), pix_code));
}

void FacilitatedPaymentsManager::OnPixCodeValidated(
    std::string pix_code,
    base::expected<bool, std::string> is_pix_code_valid) {
  if (!is_pix_code_valid.has_value()) {
    // Pix code validator encountered an error.
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kCodeValidatorFailed);
    Reset();
    return;
  }

  if (!is_pix_code_valid.value()) {
    // Pix code is not valid.
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kInvalidCode);
    Reset();
    return;
  }

  initiate_payment_request_details_->pix_code_ = std::move(pix_code);
  api_availability_check_latency_ = base::TimeTicks::Now();
  api_client_->IsAvailable(
      base::BindOnce(&FacilitatedPaymentsManager::OnApiAvailabilityReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::StartPixCodeDetectionLatencyTimer() {
  pix_code_detection_latency_measuring_timestamp_ = base::TimeTicks::Now();
}

int64_t FacilitatedPaymentsManager::GetPixCodeDetectionLatencyInMillis() const {
  return (base::TimeTicks::Now() -
          pix_code_detection_latency_measuring_timestamp_)
      .InMilliseconds();
}

void FacilitatedPaymentsManager::OnApiAvailabilityReceived(
    bool is_api_available) {
  LogIsApiAvailableResult(is_api_available, (base::TimeTicks::Now() -
                                             api_availability_check_latency_));
  if (!is_api_available) {
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kApiNotAvailable);
    Reset();
    return;
  }

  // If the personal data manager isn't available, then the flow should have
  // been abandoned already in `ProcessPixCodeDetectionResult`.
  CHECK(client_->GetPersonalDataManager());
  initiate_payment_request_details_->billing_customer_number_ =
      autofill::payments::GetBillingCustomerId(
          client_->GetPersonalDataManager());
  // Before showing the payment prompt, load the risk data required for
  // initiating payment request. The risk data is collected once per page load
  // if a PIX code was detected.
  if (initiate_payment_request_details_->risk_data_.empty()) {
    client_->LoadRiskData(
        base::BindOnce(&FacilitatedPaymentsManager::OnRiskDataLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  client_->ShowPixPaymentPrompt(
      client_->GetPersonalDataManager()
          ->payments_data_manager()
          .GetMaskedBankAccounts(),
      base::BindOnce(&FacilitatedPaymentsManager::OnPixPaymentPromptResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::OnRiskDataLoaded(
    const std::string& risk_data) {
  if (risk_data.empty()) {
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kRiskDataEmpty);
    Reset();
    return;
  }
  initiate_payment_request_details_->risk_data_ = risk_data;

  // Populating the risk data and showing the payment prompt may occur
  // asynchronously. If the user has already selected the payment account, send
  // the request to initiate payment.
  if (initiate_payment_request_details_->IsReadyForPixPayment()) {
    SendInitiatePaymentRequest();
  }
}

void FacilitatedPaymentsManager::OnPixPaymentPromptResult(
    bool is_prompt_accepted,
    int64_t selected_instrument_id) {
  if (!is_prompt_accepted) {
    Reset();
    return;
  }
  initiate_payment_request_details_->instrument_id_ = selected_instrument_id;
  get_client_token_loading_latency_ = base::TimeTicks::Now();
  api_client_->GetClientToken(
      base::BindOnce(&FacilitatedPaymentsManager::OnGetClientToken,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::OnGetClientToken(
    std::vector<uint8_t> client_token) {
  LogGetClientTokenResult(
      !client_token.empty(),
      (base::TimeTicks::Now() - get_client_token_loading_latency_));
  if (client_token.empty()) {
    Reset();
    return;
  }
  initiate_payment_request_details_->client_token_ = client_token;

  if (initiate_payment_request_details_->IsReadyForPixPayment()) {
    SendInitiatePaymentRequest();
  }
}

void FacilitatedPaymentsManager::SendInitiatePaymentRequest() {
  if (FacilitatedPaymentsNetworkInterface* payments_network_interface =
          client_->GetFacilitatedPaymentsNetworkInterface()) {
    payments_network_interface->InitiatePayment(
        std::move(initiate_payment_request_details_),
        base::BindOnce(
            &FacilitatedPaymentsManager::OnInitiatePaymentResponseReceived,
            weak_ptr_factory_.GetWeakPtr()),
        client_->GetPersonalDataManager()->app_locale());
  }
}

void FacilitatedPaymentsManager::OnInitiatePaymentResponseReceived(
    autofill::AutofillClient::PaymentsRpcResult result,
    std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
        response_details) {
  // TODO(b/300334855): Send the action token from the InitiatePayment response
  // into the purchase manager.
}

void FacilitatedPaymentsManager::ResetForTesting() {
  is_test_ = false;
  Reset();
  is_test_ = true;
}

}  // namespace payments::facilitated
