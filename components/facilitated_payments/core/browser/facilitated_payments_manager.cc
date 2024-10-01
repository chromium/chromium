// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

namespace payments::facilitated {

FacilitatedPaymentsManager::FacilitatedPaymentsManager(
    FacilitatedPaymentsDriver* driver,
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : driver_(CHECK_DEREF(driver)),
      client_(CHECK_DEREF(client)),
      api_client_creator_(std::move(api_client_creator)),
      optimization_guide_decider_(optimization_guide_decider),
      initiate_payment_request_details_(
          std::make_unique<
              FacilitatedPaymentsInitiatePaymentRequestDetails>()) {
  DCHECK(optimization_guide_decider_);
  RegisterPixAllowlist();
}

FacilitatedPaymentsManager::~FacilitatedPaymentsManager() {
  client_->DismissPrompt();
}

void FacilitatedPaymentsManager::Reset() {
  has_payflow_started_ = false;
  ukm_source_id_ = 0;
  trigger_source_ = TriggerSource::kUnknown;
  initiate_payment_request_details_ =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void FacilitatedPaymentsManager::OnPixCodeCopiedToClipboard(
    const GURL& render_frame_host_url,
    const std::string& pix_code,
    ukm::SourceId ukm_source_id) {
  if (has_payflow_started_) {
    return;
  }
  has_payflow_started_ = true;
  ukm_source_id_ = ukm_source_id;
  trigger_source_ = TriggerSource::kCopyEvent;
  // Check whether the domain for the render_frame_host_url is allowlisted.
  if (!IsMerchantAllowlisted(render_frame_host_url)) {
    // The merchant is not part of the allowlist, ignore the copy event.
    return;
  }
  initiate_payment_request_details_->merchant_payment_page_hostname_ =
      render_frame_host_url.host();
  // Trigger Pix code validation.
  utility_process_validator_.ValidatePixCode(
      pix_code, base::BindOnce(&FacilitatedPaymentsManager::OnPixCodeValidated,
                               weak_ptr_factory_.GetWeakPtr(), pix_code,
                               base::TimeTicks::Now()));
}

void FacilitatedPaymentsManager::RegisterPixAllowlist() const {
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST});
}

bool FacilitatedPaymentsManager::IsMerchantAllowlisted(const GURL& url) const {
  // Since the optimization guide decider integration corresponding to PIX
  // merchant lists are allowlists for the question "Can this site be
  // optimized?", a match on the allowlist answers the question with "yes".
  // Therefore, `kTrue` indicates that `url` is allowed for detecting PIX code
  // on copy events. If the optimization type was not registered in time when we
  // queried it, it will be `kUnknown`.
  return optimization_guide_decider_->CanApplyOptimization(
             url, optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST,
             /*optimization_metadata=*/nullptr) ==
         optimization_guide::OptimizationGuideDecision::kTrue;
}

void FacilitatedPaymentsManager::OnPixCodeValidated(
    std::string pix_code,
    base::TimeTicks start_time,
    base::expected<bool, std::string> is_pix_code_valid) {
  LogPaymentCodeValidationResultAndLatency(
      is_pix_code_valid, (base::TimeTicks::Now() - start_time));
  if (!is_pix_code_valid.has_value()) {
    // Pix code validator encountered an error.
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kCodeValidatorFailed);
    return;
  }

  if (!is_pix_code_valid.value()) {
    // Pix code is not valid.
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kInvalidCode);
    return;
  }
  // If a valid PIX code is found, and the user has Google wallet linked PIX
  // accounts, verify that the payments API is available, and then show the PIX
  // payment prompt.
  auto* payments_data_manager = client_->GetPaymentsDataManager();
  if (!payments_data_manager ||
      !payments_data_manager->IsFacilitatedPaymentsPixUserPrefEnabled() ||
      !payments_data_manager->HasMaskedBankAccounts()) {
    return;
  }

  // Pix payment flow can't be completed in the landscape mode as platform
  // doesn't support it yet.
  if (client_->IsInLandscapeMode() &&
      !base::FeatureList::IsEnabled(kEnablePixPaymentsInLandscapeMode)) {
    LogPaymentNotOfferedReason(
        PaymentNotOfferedReason::kLandscapeScreenOrientation);
    return;
  }

  if (!GetApiClient()) {
    return;
  }

  initiate_payment_request_details_->pix_code_ = std::move(pix_code);
  api_availability_check_start_time_ = base::TimeTicks::Now();
  GetApiClient()->IsAvailable(
      base::BindOnce(&FacilitatedPaymentsManager::OnApiAvailabilityReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

FacilitatedPaymentsApiClient* FacilitatedPaymentsManager::GetApiClient() {
  if (!api_client_) {
    if (api_client_creator_) {
      api_client_ = std::move(api_client_creator_).Run();
    }
  }

  return api_client_.get();
}

void FacilitatedPaymentsManager::OnApiAvailabilityReceived(
    bool is_api_available) {
  LogIsApiAvailableResult(
      is_api_available,
      (base::TimeTicks::Now() - api_availability_check_start_time_));
  if (!is_api_available) {
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kApiNotAvailable);
    return;
  }

  CHECK(client_->GetPaymentsDataManager());
  initiate_payment_request_details_->billing_customer_number_ =
      autofill::payments::GetBillingCustomerId(
          client_->GetPaymentsDataManager());

  bool promptShown = client_->ShowPixPaymentPrompt(
      client_->GetPaymentsDataManager()->GetMaskedBankAccounts(),
      base::BindOnce(&FacilitatedPaymentsManager::OnPixPaymentPromptResult,
                     weak_ptr_factory_.GetWeakPtr()));
  LogFopSelectorShown(promptShown);
  if (promptShown) {
    fop_selector_shown_time_ = base::TimeTicks::Now();
  }
}

void FacilitatedPaymentsManager::OnPixPaymentPromptResult(
    bool is_prompt_accepted,
    int64_t selected_instrument_id) {
  if (!is_prompt_accepted) {
    LogTransactionResult(TransactionResult::kAbandoned, trigger_source_,
                         base::TimeTicks::Now() - fop_selector_shown_time_,
                         ukm_source_id_);
    return;
  }

  client_->ShowProgressScreen();

  initiate_payment_request_details_->instrument_id_ = selected_instrument_id;

  client_->LoadRiskData(
      base::BindOnce(&FacilitatedPaymentsManager::OnRiskDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void FacilitatedPaymentsManager::OnRiskDataLoaded(
    base::TimeTicks start_time,
    const std::string& risk_data) {
  LogLoadRiskDataResultAndLatency(/*was_successful=*/!risk_data.empty(),
                                  base::TimeTicks::Now() - start_time);
  if (risk_data.empty()) {
    client_->ShowErrorScreen();
    LogPaymentNotOfferedReason(PaymentNotOfferedReason::kRiskDataEmpty);
    return;
  }
  initiate_payment_request_details_->risk_data_ = risk_data;

  get_client_token_loading_start_time_ = base::TimeTicks::Now();
  GetApiClient()->GetClientToken(
      base::BindOnce(&FacilitatedPaymentsManager::OnGetClientToken,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::OnGetClientToken(
    std::vector<uint8_t> client_token) {
  LogGetClientTokenResult(
      !client_token.empty(),
      (base::TimeTicks::Now() - get_client_token_loading_start_time_));
  if (client_token.empty()) {
    client_->ShowErrorScreen();
    LogTransactionResult(TransactionResult::kFailed, trigger_source_,
                         base::TimeTicks::Now() - fop_selector_shown_time_,
                         ukm_source_id_);
    return;
  }
  initiate_payment_request_details_->client_token_ = client_token;

  if (initiate_payment_request_details_->IsReadyForPixPayment()) {
    SendInitiatePaymentRequest();
  }
}

void FacilitatedPaymentsManager::SendInitiatePaymentRequest() {
  initiate_payment_network_start_time_ = base::TimeTicks::Now();
  if (FacilitatedPaymentsNetworkInterface* payments_network_interface =
          client_->GetFacilitatedPaymentsNetworkInterface()) {
    payments_network_interface->InitiatePayment(
        std::move(initiate_payment_request_details_),
        base::BindOnce(
            &FacilitatedPaymentsManager::OnInitiatePaymentResponseReceived,
            weak_ptr_factory_.GetWeakPtr()),
        client_->GetPaymentsDataManager()->app_locale());
  }
}

void FacilitatedPaymentsManager::OnInitiatePaymentResponseReceived(
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
    std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
        response_details) {
  base::TimeDelta latency =
      base::TimeTicks::Now() - initiate_payment_network_start_time_;
  if (result !=
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    LogInitiatePaymentResult(/*result=*/false, latency);
    client_->ShowErrorScreen();
    LogTransactionResult(TransactionResult::kFailed, trigger_source_,
                         base::TimeTicks::Now() - fop_selector_shown_time_,
                         ukm_source_id_);
    return;
  }
  LogInitiatePaymentResult(/*result=*/true, latency);
  DCHECK(response_details);
  if (response_details->action_token_.empty()) {
    client_->ShowErrorScreen();
    LogTransactionResult(TransactionResult::kFailed, trigger_source_,
                         base::TimeTicks::Now() - fop_selector_shown_time_,
                         ukm_source_id_);
    return;
  }
  std::optional<CoreAccountInfo> account_info = client_->GetCoreAccountInfo();
  // If the user logged out after selecting the payment method, the
  // `account_info` would be empty, and the `FacilitatedPaymentsManager` should
  // abandon the payment flow.
  if (!account_info.has_value() || account_info.value().IsEmpty()) {
    client_->ShowErrorScreen();
    LogTransactionResult(TransactionResult::kFailed, trigger_source_,
                         base::TimeTicks::Now() - fop_selector_shown_time_,
                         ukm_source_id_);
    return;
  }
  purchase_action_start_time_ = base::TimeTicks::Now();
  GetApiClient()->InvokePurchaseAction(
      account_info.value(), response_details->action_token_,
      base::BindOnce(&FacilitatedPaymentsManager::OnPurchaseActionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::OnPurchaseActionResult(
    FacilitatedPaymentsApiClient::PurchaseActionResult result) {
  // When server responds to the purchase action, Google Play Services takes
  // over, and the progress screen gets dismissed. Calling `DismissPrompt`
  // clears the associated Java objects.
  client_->DismissPrompt();
  LogInitiatePurchaseActionResult(
      /*result=*/result ==
          FacilitatedPaymentsApiClient::PurchaseActionResult::kResultOk,
      base::TimeTicks::Now() - purchase_action_start_time_);
  // Map the result received from the purchase action to overall transaction
  // result.
  TransactionResult transaction_result = TransactionResult::kFailed;
  switch (result) {
    case FacilitatedPaymentsApiClient::PurchaseActionResult::kResultOk:
      transaction_result = TransactionResult::kSuccess;
      break;
    case FacilitatedPaymentsApiClient::PurchaseActionResult::kCouldNotInvoke:
      transaction_result = TransactionResult::kFailed;
      break;
    case FacilitatedPaymentsApiClient::PurchaseActionResult::kResultCanceled:
      transaction_result = TransactionResult::kAbandoned;
      break;
  }
  LogTransactionResult(transaction_result, trigger_source_,
                       base::TimeTicks::Now() - fop_selector_shown_time_,
                       ukm_source_id_);
}

}  // namespace payments::facilitated
