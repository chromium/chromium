// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/payment_link_manager.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/browser/strike_databases/payment_link_suggestion_strike_database.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/facilitated_payments/core/validation/payment_link_validator.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "url/gurl.h"

namespace payments::facilitated {
namespace {

static constexpr FacilitatedPaymentsType kPaymentsType =
    FacilitatedPaymentsType::kEwallet;

static constexpr base::TimeDelta kProgressScreenDismissDelay = base::Seconds(1);

}  // namespace

PaymentLinkManager::PaymentLinkManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : client_(CHECK_DEREF(client)),
      api_client_creator_(api_client_creator),
      optimization_guide_decider_(CHECK_DEREF(optimization_guide_decider)) {}

PaymentLinkManager::~PaymentLinkManager() {
  DismissPrompt();
}

void PaymentLinkManager::TriggerPaymentLinkPushPayment(
    const GURL& payment_link_url,
    const GURL& page_url,
    ukm::SourceId ukm_source_id) {
  payment_flow_triggered_timestamp_ = base::TimeTicks::Now();
  ukm_source_id_ = ukm_source_id;
  LogPaymentLinkDetected(ukm_source_id_);

  scheme_ = PaymentLinkValidator().GetScheme(payment_link_url);
  if (scheme_ == PaymentLinkValidator::Scheme::kInvalid) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kLinkIsInvalid);
    return;
  }

  if (client_->IsFoldable()) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kFoldableDevice,
                               scheme_);
    return;
  }

  if (auto* strike_database = GetOrCreateStrikeDatabase()) {
    if (strike_database->ShouldBlockFeature()) {
      LogEwalletFlowExitedReason(EwalletFlowExitedReason::kMaxStrikes, scheme_);
      return;
    }
  }

  initiate_payment_request_details_ =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  initiate_payment_request_details_->merchant_payment_page_hostname_ =
      page_url.GetHost();
  initiate_payment_request_details_->payment_link_ = payment_link_url.spec();

  client_->SetUiEventListener(base::BindRepeating(
      &PaymentLinkManager::OnUiScreenEvent, weak_ptr_factory_.GetWeakPtr()));

  if (CanTriggerEwalletPaymentFlow(page_url)) {
    RetrieveSupportedEwallets(payment_link_url);
  }

  std::unique_ptr<FacilitatedPaymentsAppInfoList> supported_apps;
  if (CanTriggerAppPaymentFlow(page_url)) {
    supported_apps = client_->GetDeviceDelegate()->GetSupportedPaymentApps(
        PaymentLinkValidator::SanitizeForPaymentAppRetrieval(payment_link_url));
    if ((!supported_apps || supported_apps->Size() == 0) &&
        base::FeatureList::IsEnabled(
            payments::facilitated::kFacilitatedPaymentsEnableA2APayment)) {
      LogA2APayflowExitedReason(A2AFlowExitedReason::kNoSupportedPaymentApp,
                                scheme_);
    }
  }

  if (!base::FeatureList::IsEnabled(
          payments::facilitated::kFacilitatedPaymentsEnableA2APayment)) {
    LogA2APayflowExitedReason(A2AFlowExitedReason::kFlagNotEnabled, scheme_);
    supported_apps.reset();
  }

  ShowPaymentLinkPrompt(supported_ewallets_, std::move(supported_apps),
                        base::BindOnce(&PaymentLinkManager::OnFopSelected,
                                       weak_ptr_factory_.GetWeakPtr()));
}

bool PaymentLinkManager::CanTriggerEwalletPaymentFlow(const GURL& page_url) {
  if (optimization_guide_decider_->CanApplyOptimization(
          page_url, optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST,
          /*optimization_metadata=*/nullptr) !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    // The merchant is not part of the allowlist, ignore the eWallet push
    // payment.
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kNotInAllowlist);
    return false;
  }

  // Ewallet payment flow can't be completed in the landscape mode as the
  // Payments server doesn't support it yet.
  if (client_->IsInLandscapeMode()) {
    LogEwalletFlowExitedReason(
        EwalletFlowExitedReason::kLandscapeScreenOrientation, scheme_);
    return false;
  }

  base::TimeTicks api_availability_check_start_time = base::TimeTicks::Now();
  bool is_api_available =
      GetApiClient() != nullptr && GetApiClient()->IsAvailableSync();
  LogApiAvailabilityCheckResultAndLatency(
      kPaymentsType, is_api_available,
      (base::TimeTicks::Now() - api_availability_check_start_time), scheme_);

  if (!is_api_available) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kApiClientNotAvailable,
                               scheme_);
    return false;
  }

  autofill::PaymentsDataManager* payments_data_manager =
      client_->GetPaymentsDataManager();
  if (!payments_data_manager) {
    // Payments data manager can be null only in tests.
    return false;
  }

  initiate_payment_request_details_->billing_customer_number_ =
      autofill::payments::GetBillingCustomerId(*payments_data_manager);

  if (!payments_data_manager->IsFacilitatedPaymentsEwalletUserPrefEnabled()) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kUserOptedOut, scheme_);
    return false;
  }

  return true;
}

void PaymentLinkManager::RetrieveSupportedEwallets(
    const GURL& payment_link_url) {
  base::span<const autofill::Ewallet> ewallet_accounts =
      client_->GetPaymentsDataManager()->GetEwalletAccounts();
  supported_ewallets_.reserve(ewallet_accounts.size());
  std::ranges::copy_if(
      ewallet_accounts, std::back_inserter(supported_ewallets_),
      [&payment_link_url](const autofill::Ewallet& ewallet) {
        return ewallet.SupportsPaymentLink(payment_link_url.spec());
      });

  if (supported_ewallets_.size() == 0) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kNoSupportedEwallet,
                               scheme_);
    return;
  }
}

bool PaymentLinkManager::CanTriggerAppPaymentFlow(const GURL& page_url) {
  if (optimization_guide_decider_->CanApplyOptimization(
          page_url, optimization_guide::proto::A2A_MERCHANT_ALLOWLIST,
          /*optimization_metadata=*/nullptr) !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    LogA2APayflowExitedReason(A2AFlowExitedReason::kNotInAllowlist);
    return false;
  }

  if (!client_->GetPaymentsDataManager()) {
    // Payments data manager can be null only in tests.
    return false;
  }

  if (!client_->GetPaymentsDataManager()
           ->IsFacilitatedPaymentsA2AUserPrefEnabled()) {
    LogA2APayflowExitedReason(A2AFlowExitedReason::kUserOptedOut, scheme_);
    return false;
  }

  return true;
}

void PaymentLinkManager::Reset() {
  supported_ewallets_.clear();
  ukm_source_id_ = ukm::kInvalidSourceId;
  initiate_payment_request_details_.reset();
  ui_state_ = UiState::kHidden;
  is_ewallet_available_ = false;
  is_payment_app_available_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

FacilitatedPaymentsApiClient* PaymentLinkManager::GetApiClient() {
  if (!api_client_) {
    if (api_client_creator_) {
      api_client_ = api_client_creator_.Run();
    }
  }

  return api_client_.get();
}

void PaymentLinkManager::OnFopSelected(SelectedFopData selected_fop_data) {
  switch (selected_fop_data.fop_type) {
    case FopType::kGPayInstrument:
      // GPayInstrument are Pix and eWallet, in PaymentLinkManger only eWallet
      // and Payment app are handled.
      OnEwalletAccountSelected(selected_fop_data.instrument_id);
      break;
    case FopType::kExternalPaymentApp:
      OnPaymentAppSelected(selected_fop_data.package_name,
                           selected_fop_data.activity_name);
      break;
  }
}

void PaymentLinkManager::OnEwalletAccountSelected(
    int64_t selected_instrument_id) {
  if (auto* strike_database = GetOrCreateStrikeDatabase()) {
    strike_database->ClearStrikes();
  }

  LogEwalletFopSelected(GetAvailableEwalletsConfiguration());
  LogEwalletFopSelectorResultUkm(/*accepted=*/true, ukm_source_id_, scheme_);
  LogNonCardPaymentMethodsFopSelected(
      GetPaymentLinkFopSelectorType(),
      PaymentLinkFopSelectorAction::kEwalletSelected, scheme_);
  if (is_payment_app_available_) {
    LogA2APayflowExitedReason(A2AFlowExitedReason::kOtherFopSelected, scheme_);
  }

  ShowProgressScreen();

  initiate_payment_request_details_->instrument_id_ = selected_instrument_id;
  auto iter_ewallet = std::ranges::find_if(
      supported_ewallets_, [&](const autofill::Ewallet& ewallet) {
        return ewallet.payment_instrument().instrument_id() ==
               selected_instrument_id;
      });
  CHECK(iter_ewallet != supported_ewallets_.end());
  is_device_bound_for_logging_ =
      (*iter_ewallet).payment_instrument().is_fido_enrolled();

  client_->LoadRiskData(base::BindOnce(&PaymentLinkManager::OnRiskDataLoaded,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       base::TimeTicks::Now()));
}

void PaymentLinkManager::OnPaymentAppSelected(std::string_view package_name,
                                              std::string_view activity_name) {
  if (auto* strike_database = GetOrCreateStrikeDatabase()) {
    strike_database->ClearStrikes();
  }

  LogNonCardPaymentMethodsFopSelected(
      GetPaymentLinkFopSelectorType(),
      PaymentLinkFopSelectorAction::kPaymentAppSelected, scheme_);

  if (is_ewallet_available_) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kOtherFopSelected,
                               scheme_);
  }

  bool result = client_->GetDeviceDelegate()->InvokePaymentApp(
      package_name, activity_name,
      GURL(initiate_payment_request_details_->payment_link_));

  LogInvokePaymentAppResultAndLatency(
      result, base::TimeTicks::Now() - payment_flow_triggered_timestamp_,
      scheme_);

  DismissPrompt();
}

void PaymentLinkManager::OnRiskDataLoaded(base::TimeTicks start_time,
                                          const std::string& risk_data) {
  LogLoadRiskDataResultAndLatency(kPaymentsType,
                                  /*was_successful=*/!risk_data.empty(),
                                  base::TimeTicks::Now() - start_time, scheme_);
  if (risk_data.empty()) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kRiskDataEmpty,
                               scheme_);
    ShowErrorScreen();
    return;
  }

  initiate_payment_request_details_->risk_data_ = risk_data;

  GetApiClient()->GetClientToken(
      base::BindOnce(&PaymentLinkManager::OnGetClientToken,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void PaymentLinkManager::OnGetClientToken(base::TimeTicks start_time,
                                          std::vector<uint8_t> client_token) {
  LogGetClientTokenResultAndLatency(kPaymentsType, !client_token.empty(),
                                    (base::TimeTicks::Now() - start_time),
                                    scheme_);
  if (client_token.empty()) {
    LogEwalletFlowExitedReason(
        EwalletFlowExitedReason::kClientTokenNotAvailable, scheme_);
    ShowErrorScreen();
    return;
  }
  initiate_payment_request_details_->client_token_ = std::move(client_token);

  SendInitiatePaymentRequest();
}

void PaymentLinkManager::SendInitiatePaymentRequest() {
  FacilitatedPaymentsNetworkInterface* payments_network_interface =
      client_->GetFacilitatedPaymentsNetworkInterface();

  if (!payments_network_interface) {
    ShowErrorScreen();
    return;
  }

  LogInitiatePaymentAttempt(kPaymentsType, scheme_);
  payments_network_interface->InitiatePayment(
      std::move(initiate_payment_request_details_),
      base::BindOnce(&PaymentLinkManager::OnInitiatePaymentResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      client_->GetPaymentsDataManager()->app_locale());
}

void PaymentLinkManager::OnInitiatePaymentResponseReceived(
    base::TimeTicks start_time,
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
    std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
        response_details) {
  bool is_successful =
      result ==
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess;
  LogInitiatePaymentResultAndLatency(kPaymentsType, /*result=*/is_successful,
                                     base::TimeTicks::Now() - start_time,
                                     scheme_);
  if (!is_successful) {
    ShowErrorScreen();
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kInitiatePaymentFailed,
                               scheme_);
    return;
  }
  if (!response_details ||
      response_details->secure_payload_.action_token.empty()) {
    LogEwalletFlowExitedReason(
        EwalletFlowExitedReason::kActionTokenNotAvailable, scheme_);
    ShowErrorScreen();
    return;
  }
  std::optional<CoreAccountInfo> account_info = client_->GetCoreAccountInfo();
  // If the user logged out after selecting the payment method, the
  // `account_info` would be empty, and the  the payment flow should be
  // abandoned.
  if (!account_info.has_value() || account_info.value().IsEmpty()) {
    LogEwalletFlowExitedReason(EwalletFlowExitedReason::kUserLoggedOut,
                               scheme_);
    ShowErrorScreen();
    return;
  }

  LogInitiatePurchaseActionAttempt(kPaymentsType, scheme_);
  GetApiClient()->InvokePurchaseAction(
      account_info.value(), response_details->secure_payload_,
      base::BindOnce(&PaymentLinkManager::OnTransactionResult,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));

  // Close the progress screen just after the platform screen appears.
  ui_timer_.Start(FROM_HERE, kProgressScreenDismissDelay,
                  base::BindOnce(&PaymentLinkManager::DismissProgressScreen,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void PaymentLinkManager::OnTransactionResult(base::TimeTicks start_time,
                                             PurchaseActionResult result) {
  switch (result) {
    case PurchaseActionResult::kCouldNotInvoke:
      ShowErrorScreen();
      break;
    case PurchaseActionResult::kResultOk:
      [[fallthrough]];  // Intentional fallthrough.
    case PurchaseActionResult::kResultCanceled:
      DismissPrompt();
      break;
  }

  LogEwalletInitiatePurchaseActionResultAndLatency(
      result, base::TimeTicks::Now() - start_time, scheme_,
      is_device_bound_for_logging_);
}

void PaymentLinkManager::OnUiScreenEvent(UiEvent ui_event_type) {
  switch (ui_event_type) {
    case UiEvent::kNewScreenShown: {
      CHECK_NE(ui_state_, UiState::kHidden);
      LogUiScreenShown(kPaymentsType, ui_state_, scheme_);
      if (ui_state_ == UiState::kFopSelector) {
        if (is_ewallet_available_) {
          // LogFopSelectorShownLatency is used to log latency for eWallet and
          // Pix. Pix payment methods is not handled by PaymentLinkManager.
          LogFopSelectorShownLatency(
              FacilitatedPaymentsType::kEwallet,
              base::TimeTicks::Now() - payment_flow_triggered_timestamp_,
              scheme_);
        }
        // LogPaymentLinkFopSelectorShownLatency is used to log latency for both
        // eWallet and A2A. PaymentLinkManager handles payment via eWallet and
        // Payment app.
        LogPaymentLinkFopSelectorShownLatency(
            GetPaymentLinkFopSelectorType(),
            base::TimeTicks::Now() - payment_flow_triggered_timestamp_,
            scheme_);
        LogEwalletFopSelectorShownUkm(ukm_source_id_, scheme_);
      }
      break;
    }
    case UiEvent::kScreenCouldNotBeShown:
      // TODO(crbug.com/427597144): Handle the "failure to show" case separately
      // if required.
      [[fallthrough]];  // Intentional fallthrough.
    case UiEvent::kScreenClosedNotByUser: {
      if (ui_state_ == UiState::kFopSelector) {
        if (is_ewallet_available_) {
          LogEwalletFlowExitedReason(
              EwalletFlowExitedReason::kFopSelectorClosedNotByUser, scheme_);
        }
        if (is_payment_app_available_) {
          LogA2APayflowExitedReason(
              A2AFlowExitedReason::kFopSelectorClosedNotByUser, scheme_);
        }
      }
      ui_state_ = UiState::kHidden;
      break;
    }
    case UiEvent::kScreenClosedByUser: {
      if (ui_state_ == UiState::kFopSelector) {
        if (auto* strike_database = GetOrCreateStrikeDatabase()) {
          strike_database->AddStrike();
        }
        if (is_ewallet_available_) {
          LogEwalletFlowExitedReason(
              EwalletFlowExitedReason::kFopSelectorClosedByUser, scheme_);
          LogEwalletFopSelectorResultUkm(/*accepted=*/false, ukm_source_id_,
                                         scheme_);
        }
        if (is_payment_app_available_) {
          LogA2APayflowExitedReason(
              A2AFlowExitedReason::kFopSelectorClosedByUser, scheme_);
        }
      }
      ui_state_ = UiState::kHidden;
      break;
    }
  }
}

void PaymentLinkManager::DismissPrompt() {
  ui_state_ = UiState::kHidden;
  client_->DismissPrompt();
}

void PaymentLinkManager::ShowPaymentLinkPrompt(
    base::span<const autofill::Ewallet> ewallet_suggestions,
    std::unique_ptr<FacilitatedPaymentsAppInfoList> app_suggestions,
    base::OnceCallback<void(payments::facilitated::SelectedFopData)>
        on_fop_selected) {
  is_ewallet_available_ = ewallet_suggestions.size() > 0;
  is_payment_app_available_ =
      (app_suggestions != nullptr) && app_suggestions->Size() > 0;

  if (!is_ewallet_available_ && !is_payment_app_available_) {
    return;
  }
  if (is_payment_app_available_) {
    client_->GetPaymentsDataManager()->SetFacilitatedPaymentsA2ATriggeredOnce(
        true);
  }

  ui_state_ = UiState::kFopSelector;
  client_->ShowPaymentLinkPrompt(std::move(ewallet_suggestions),
                                 std::move(app_suggestions),
                                 std::move(on_fop_selected));
}

void PaymentLinkManager::ShowProgressScreen() {
  ui_state_ = UiState::kProgressScreen;
  client_->ShowProgressScreen();
}

void PaymentLinkManager::ShowErrorScreen() {
  ui_state_ = UiState::kErrorScreen;
  client_->ShowErrorScreen();
}

AvailableEwalletsConfiguration
PaymentLinkManager::GetAvailableEwalletsConfiguration() {
  if (supported_ewallets_.size() == 1) {
    return supported_ewallets_[0].payment_instrument().is_fido_enrolled()
               ? AvailableEwalletsConfiguration::kSingleBoundEwallet
               : AvailableEwalletsConfiguration::kSingleUnboundEwallet;
  }
  return AvailableEwalletsConfiguration::kMultipleEwallets;
}

void PaymentLinkManager::DismissProgressScreen() {
  if (ui_state_ == UiState::kProgressScreen) {
    DismissPrompt();
  }
}

PaymentLinkFopSelectorTypes
PaymentLinkManager::GetPaymentLinkFopSelectorType() {
  if (is_payment_app_available_ && is_ewallet_available_) {
    return PaymentLinkFopSelectorTypes::kEwalletAndA2A;
  }
  if (is_payment_app_available_) {
    return PaymentLinkFopSelectorTypes::kA2AOnly;
  }
  if (is_ewallet_available_) {
    return PaymentLinkFopSelectorTypes::kEwalletOnly;
  }
  NOTREACHED();
}

PaymentLinkSuggestionStrikeDatabase*
PaymentLinkManager::GetOrCreateStrikeDatabase() {
  if (!strike_database_) {
    if (auto* strike_database = client_->GetStrikeDatabase()) {
      strike_database_ = std::make_unique<PaymentLinkSuggestionStrikeDatabase>(
          strike_database);
    }
  }
  return strike_database_.get();
}

}  // namespace payments::facilitated
