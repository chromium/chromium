// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/network_api/multiple_request_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"

namespace payments::facilitated {
namespace {

static constexpr base::TimeDelta kProgressScreenDismissDelay = base::Seconds(2);
static constexpr FacilitatedPaymentsType kPaymentsType =
    FacilitatedPaymentsType::kPix;

}  // namespace

PixManager::PixManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : client_(CHECK_DEREF(client)),
      api_client_creator_(api_client_creator),
      optimization_guide_decider_(optimization_guide_decider),
      initiate_payment_request_details_(
          std::make_unique<
              FacilitatedPaymentsInitiatePaymentRequestDetails>()) {
  DCHECK(optimization_guide_decider_);
}

PixManager::~PixManager() {
  DismissPrompt();
}

void PixManager::Reset() {
  has_payflow_started_ = false;
  ukm_source_id_ = 0;
  initiate_payment_request_details_ =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  ui_state_ = UiState::kHidden;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void PixManager::OnPixCodeCopiedToClipboard(const GURL& render_frame_host_url,
                                            const std::string& pix_code,
                                            ukm::SourceId ukm_source_id) {
  if (has_payflow_started_) {
    return;
  }
  has_payflow_started_ = true;
  client_->SetUiEventListener(base::BindRepeating(
      &PixManager::OnUiEvent, weak_ptr_factory_.GetWeakPtr()));
  pix_code_copied_timestamp_ = base::TimeTicks::Now();
  ukm_source_id_ = ukm_source_id;
  // Check whether the domain for the render_frame_host_url is allowlisted.
  if (!IsMerchantAllowlisted(render_frame_host_url)) {
    // The merchant is not part of the allowlist, ignore the copy event.
    return;
  }
  LogPixCodeCopied(ukm_source_id_);
  initiate_payment_request_details_->merchant_payment_page_hostname_ =
      render_frame_host_url.host();
  // Trigger Pix code validation.
  utility_process_validator_.ValidatePixCode(
      pix_code, base::BindOnce(&PixManager::OnPixCodeValidated,
                               weak_ptr_factory_.GetWeakPtr(), pix_code,
                               base::TimeTicks::Now()));
}

bool PixManager::IsMerchantAllowlisted(const GURL& url) const {
  if (base::FeatureList::IsEnabled(
          kDisableFacilitatedPaymentsMerchantAllowlist)) {
    // If the merchant allowlist check is disabled, simply return true. This is
    // mainly used for manual testing of new domains before being added to the
    // allowlist.
    return true;
  }
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

void PixManager::OnPixCodeValidated(
    std::string pix_code,
    base::TimeTicks start_time,
    base::expected<bool, std::string> is_pix_code_valid) {
  LogPaymentCodeValidationResultAndLatency(
      is_pix_code_valid, (base::TimeTicks::Now() - start_time));
  if (!is_pix_code_valid.has_value()) {
    // Pix code validator encountered an error.
    LogPixFlowExitedReason(PixFlowExitedReason::kCodeValidatorFailed);
    return;
  }

  if (!is_pix_code_valid.value()) {
    // Pix code is not valid.
    LogPixFlowExitedReason(PixFlowExitedReason::kInvalidCode);
    return;
  }
  // If a valid PIX code is found, and the user has Google wallet linked PIX
  // accounts, verify that the payments API is available, and then show the PIX
  // payment prompt.
  auto* payments_data_manager = client_->GetPaymentsDataManager();
  if (!payments_data_manager) {
    // `payments_data_manager` (owned by a PersonalDataManager) does not exist
    // in a system profile but Pix should not be triggered there. Keep this
    // check for safety but no logging should be required.
    return;
  }

  // If the user has no linked Pix accounts, initialize the Pix account linking
  // flow.
  if (!payments_data_manager->HasMaskedBankAccounts()) {
    LogPixFlowExitedReason(PixFlowExitedReason::kNoLinkedAccount);
    if (base::FeatureList::IsEnabled(kEnablePixAccountLinking)) {
      client_->InitPixAccountLinkingFlow();
    }
    return;
  }

  // Pix pref is shown only if the user has linked Pix accounts.
  if (!payments_data_manager->IsFacilitatedPaymentsPixUserPrefEnabled()) {
    LogPixFlowExitedReason(PixFlowExitedReason::kUserOptedOut);
    return;
  }

  // Pix payment flow can't be completed in the landscape mode as platform
  // doesn't support it yet.
  if (client_->IsInLandscapeMode() &&
      !base::FeatureList::IsEnabled(kEnablePixPaymentsInLandscapeMode)) {
    LogPixFlowExitedReason(PixFlowExitedReason::kLandscapeScreenOrientation);
    return;
  }

  if (!GetApiClient()) {
    return;
  }

  initiate_payment_request_details_->pix_code_ = std::move(pix_code);
  GetApiClient()->IsAvailable(
      base::BindOnce(&PixManager::OnApiAvailabilityReceived,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

FacilitatedPaymentsApiClient* PixManager::GetApiClient() {
  if (!api_client_) {
    if (api_client_creator_) {
      api_client_ = api_client_creator_.Run();
    }
  }

  return api_client_.get();
}

void PixManager::OnApiAvailabilityReceived(base::TimeTicks start_time,
                                           bool is_api_available) {
  LogApiAvailabilityCheckResultAndLatency(
      kPaymentsType, is_api_available, (base::TimeTicks::Now() - start_time));
  if (!is_api_available) {
    LogPixFlowExitedReason(PixFlowExitedReason::kApiClientNotAvailable);
    return;
  }

  initiate_payment_request_details_->billing_customer_number_ =
      autofill::payments::GetBillingCustomerId(
          CHECK_DEREF(client_->GetPaymentsDataManager()));

  ShowPixPaymentPrompt(
      client_->GetPaymentsDataManager()->GetMaskedBankAccounts(),
      base::BindOnce(&PixManager::OnPixAccountSelected,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void PixManager::OnPixAccountSelected(
    base::TimeTicks fop_selector_shown_timestamp,
    int64_t selected_instrument_id) {
  LogPixFopSelectedAndLatency(base::TimeTicks::Now() -
                              fop_selector_shown_timestamp);
  LogPixFopSelectorResultUkm(/*accepted=*/true, ukm_source_id_);
  ShowProgressScreen();

  initiate_payment_request_details_->instrument_id_ = selected_instrument_id;

  client_->LoadRiskData(base::BindOnce(&PixManager::OnRiskDataLoaded,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       base::TimeTicks::Now()));
}

void PixManager::OnRiskDataLoaded(base::TimeTicks start_time,
                                  const std::string& risk_data) {
  LogLoadRiskDataResultAndLatency(kPaymentsType,
                                  /*was_successful=*/!risk_data.empty(),
                                  base::TimeTicks::Now() - start_time);
  if (risk_data.empty()) {
    ShowErrorScreen();
    LogPixFlowExitedReason(PixFlowExitedReason::kRiskDataNotAvailable);
    return;
  }
  initiate_payment_request_details_->risk_data_ = risk_data;

  GetApiClient()->GetClientToken(base::BindOnce(&PixManager::OnGetClientToken,
                                                weak_ptr_factory_.GetWeakPtr(),
                                                base::TimeTicks::Now()));
}

void PixManager::OnGetClientToken(base::TimeTicks start_time,
                                  std::vector<uint8_t> client_token) {
  LogGetClientTokenResultAndLatency(kPaymentsType, !client_token.empty(),
                                    (base::TimeTicks::Now() - start_time));
  if (client_token.empty()) {
    ShowErrorScreen();
    LogPixFlowExitedReason(PixFlowExitedReason::kClientTokenNotAvailable);
    return;
  }
  initiate_payment_request_details_->client_token_ = client_token;

  if (initiate_payment_request_details_->IsReadyForPixPayment()) {
    SendInitiatePaymentRequest();
  }
}

void PixManager::SendInitiatePaymentRequest() {
  if (base::FeatureList::IsEnabled(
          kSupportMultipleServerRequestsForPixPayments)) {
    if (auto* payments_network_interface =
            client_->GetMultipleRequestFacilitatedPaymentsNetworkInterface()) {
      LogInitiatePaymentAttempt(kPaymentsType);
      payments_network_interface->InitiatePayment(
          std::move(initiate_payment_request_details_),
          base::BindOnce(&PixManager::OnInitiatePaymentResponseReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::TimeTicks::Now()),
          client_->GetPaymentsDataManager()->app_locale());
    }
  } else {
    if (auto* payments_network_interface =
            client_->GetFacilitatedPaymentsNetworkInterface()) {
      LogInitiatePaymentAttempt(kPaymentsType);
      payments_network_interface->InitiatePayment(
          std::move(initiate_payment_request_details_),
          base::BindOnce(&PixManager::OnInitiatePaymentResponseReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::TimeTicks::Now()),
          client_->GetPaymentsDataManager()->app_locale());
    }
  }
}

void PixManager::OnInitiatePaymentResponseReceived(
    base::TimeTicks start_time,
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
    std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
        response_details) {
  base::TimeDelta latency = base::TimeTicks::Now() - start_time;
  if (result !=
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    LogInitiatePaymentResultAndLatency(kPaymentsType, /*result=*/false,
                                       latency);
    LogPixFlowExitedReason(PixFlowExitedReason::kInitiatePaymentFailed);
    ShowErrorScreen();
    return;
  }
  LogInitiatePaymentResultAndLatency(kPaymentsType, /*result=*/true, latency);

  DCHECK(response_details);
  if (response_details->secure_payload_.action_token.empty()) {
    LogPixFlowExitedReason(PixFlowExitedReason::kActionTokenNotAvailable);
    ShowErrorScreen();
    return;
  }
  std::optional<CoreAccountInfo> account_info = client_->GetCoreAccountInfo();
  // If the user logged out after selecting the payment method, the
  // `account_info` would be empty, and the `PixManager` should
  // abandon the payment flow.
  if (!account_info.has_value() || account_info.value().IsEmpty()) {
    LogPixFlowExitedReason(PixFlowExitedReason::kUserLoggedOut);
    ShowErrorScreen();
    return;
  }

  LogInitiatePurchaseActionAttempt(kPaymentsType);
  GetApiClient()->InvokePurchaseAction(
      account_info.value(), response_details->secure_payload_,
      base::BindOnce(&PixManager::OnPurchaseActionResult,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));

  // Close the progress screen just after the platform screen appears.
  ui_timer_.Start(FROM_HERE, kProgressScreenDismissDelay,
                  base::BindOnce(&PixManager::DismissProgressScreen,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void PixManager::OnPurchaseActionResult(base::TimeTicks start_time,
                                        PurchaseActionResult result) {
  switch (result) {
    case PurchaseActionResult::kCouldNotInvoke:
      LogPixFlowExitedReason(
          PixFlowExitedReason::kPurchaseActionCouldNotBeInvoked);
      ShowErrorScreen();
      break;
    case PurchaseActionResult::kResultOk:
      [[fallthrough]];  // Intentional fallthrough.
    case PurchaseActionResult::kResultCanceled:
      DismissPrompt();
      break;
  }
  // Log the general histograms.
  LogPixInitiatePurchaseActionResultAndLatency(
      result, base::TimeTicks::Now() - start_time);
  LogInitiatePurchaseActionResultUkm(result, ukm_source_id_);
  LogPixTransactionResultAndLatency(
      result, base::TimeTicks::Now() - pix_code_copied_timestamp_);
}

void PixManager::OnUiEvent(UiEvent ui_event_type) {
  switch (ui_event_type) {
    case UiEvent::kNewScreenShown: {
      CHECK_NE(ui_state_, UiState::kHidden);
      LogUiScreenShown(kPaymentsType, ui_state_);
      if (ui_state_ == UiState::kFopSelector) {
        LogFopSelectorShownLatency(
            kPaymentsType, base::TimeTicks::Now() - pix_code_copied_timestamp_);
        LogPixFopSelectorShownUkm(ukm_source_id_);
      }
      break;
    }
    case UiEvent::kScreenClosedNotByUser: {
      if (ui_state_ == UiState::kFopSelector) {
        LogPixFlowExitedReason(
            PixFlowExitedReason::kFopSelectorClosedNotByUser);
      }
      ui_state_ = UiState::kHidden;
      break;
    }
    case UiEvent::kScreenClosedByUser: {
      if (ui_state_ == UiState::kFopSelector) {
        LogPixFlowExitedReason(PixFlowExitedReason::kFopSelectorClosedByUser);
        LogPixFopSelectorResultUkm(/*accepted=*/false, ukm_source_id_);
      }
      ui_state_ = UiState::kHidden;
      break;
    }
  }
}

void PixManager::DismissPrompt() {
  ui_state_ = UiState::kHidden;
  client_->DismissPrompt();
}

void PixManager::ShowPixPaymentPrompt(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(int64_t)> on_pix_account_selected) {
  ui_state_ = UiState::kFopSelector;
  client_->ShowPixPaymentPrompt(std::move(bank_account_suggestions),
                                std::move(on_pix_account_selected));
}

void PixManager::ShowProgressScreen() {
  ui_state_ = UiState::kProgressScreen;
  client_->ShowProgressScreen();
}

void PixManager::ShowErrorScreen() {
  ui_state_ = UiState::kErrorScreen;
  client_->ShowErrorScreen();
}

void PixManager::DismissProgressScreen() {
  if (ui_state_ == UiState::kProgressScreen) {
    DismissPrompt();
  }
}

}  // namespace payments::facilitated
