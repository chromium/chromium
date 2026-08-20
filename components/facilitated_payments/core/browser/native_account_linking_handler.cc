// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/native_account_linking_handler.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

namespace payments::facilitated {

NativeAccountLinkingHandler::NativeAccountLinkingHandler(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator)
    : client_(CHECK_DEREF(client)),
      api_client_creator_(std::move(api_client_creator)) {}

NativeAccountLinkingHandler::~NativeAccountLinkingHandler() = default;

bool NativeAccountLinkingHandler::CanPromptUser() {
  strike_database::StrikeDatabaseIntegratorBase* strike_db =
      GetStrikeDatabase();
  if (strike_db) {
    using StrikeDecision =
        strike_database::StrikeDatabaseIntegratorBase::StrikeDatabaseDecision;
    auto decision = strike_db->GetStrikeDatabaseDecision();
    switch (decision) {
      case StrikeDecision::kDoNotBlock:
        break;
      case StrikeDecision::kMaxStrikeLimitReached:
        LogAccountLinkingFlowExitedReason(
            GetHistogramSuffix(), AccountLinkingFlowExitedReason::kMaxStrikes);
        return false;
      case StrikeDecision::kRequiredDelayNotPassed:
        LogAccountLinkingFlowExitedReason(
            GetHistogramSuffix(),
            AccountLinkingFlowExitedReason::kRequiredDelayNotPassed);
        return false;
    }
  }

  if (!IsUserPrefEnabled()) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(), AccountLinkingFlowExitedReason::kUserOptedOut);
    return false;
  }

  if (!client()->HasScreenlockOrBiometricSetup()) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(),
        AccountLinkingFlowExitedReason::kNoScreenlockOrBiometricSetup);
    return false;
  }

  return true;
}

void NativeAccountLinkingHandler::FetchClientToken() {
  if (!GetApiClient()) {
    OnAccountLinkingResult(AccountLinkingResult{});
    return;
  }
  GetApiClient()->GetClientToken(
      base::BindOnce(&NativeAccountLinkingHandler::OnClientTokenReceived,
                     GetWeakPtr(), base::TimeTicks::Now()));
}

void NativeAccountLinkingHandler::OnClientTokenReceived(
    base::TimeTicks start_time,
    std::vector<uint8_t> client_token) {
  bool is_client_token_received = !client_token.empty();
  LogAccountLinkingGetClientTokenResultAndLatency(
      GetHistogramSuffix(), is_client_token_received,
      base::TimeTicks::Now() - start_time);

  if (!is_client_token_received) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(),
        AccountLinkingFlowExitedReason::kClientTokenNotAvailable);
    OnAccountLinkingResult(AccountLinkingResult{});
    return;
  }

  DoOnClientTokenReceived(client_token);
}

void NativeAccountLinkingHandler::OnAccountLinkingResult(
    AccountLinkingResult result) {
  // Sanitize the response: GMSCore returning success but omitting a valid
  // instrument ID indicates a silent API breakdown. Surface this as a strict
  // failure directly so telemetry picks up the generic failure reason.
  if (result.is_successful && result.instrument_id <= 0) {
    result.is_successful = false;
    result.error_code = AccountLinkingResultCode::kResultError;
  }

  if (!result.is_successful) {
    if (result.error_code == AccountLinkingResultCode::kResultCanceled) {
      LogAccountLinkingFlowExitedReason(
          GetHistogramSuffix(),
          AccountLinkingFlowExitedReason::kUserCanceledInGmsCore);
    } else if (result.error_code == AccountLinkingResultCode::kResultError) {
      LogAccountLinkingFlowExitedReason(
          GetHistogramSuffix(),
          AccountLinkingFlowExitedReason::kGmsCoreFlowFailed);
    }
  }
  DoOnAccountLinkingResult(result);
}

void NativeAccountLinkingHandler::InitiateAccountLinkingNetworkCall(
    const std::vector<uint8_t>& client_token) {
  auto* payments_network_interface =
      client_->GetFacilitatedPaymentsNetworkInterface();
  if (!payments_network_interface) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(),
        AccountLinkingFlowExitedReason::kNetworkInterfaceUnavailable);
    OnAccountLinkingResult(AccountLinkingResult{});
    return;
  }

  auto billing_customer_id = autofill::payments::GetBillingCustomerId(
      CHECK_DEREF(client_->GetPaymentsDataManager()));

  payments_network_interface->GetDetailsForCreatePaymentInstrument(
      billing_customer_id, client_token,
      base::BindOnce(&NativeAccountLinkingHandler::
                         OnGetDetailsForCreatePaymentInstrumentResponseReceived,
                     GetWeakPtr(), base::TimeTicks::Now()),
      client_->GetPaymentsDataManager()->app_locale());
}

void NativeAccountLinkingHandler::InvokeInstrumentManager(
    CoreAccountInfo primary_account,
    const std::vector<uint8_t>& action_token) {
  if (!GetApiClient()) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(),
        AccountLinkingFlowExitedReason::kApiClientNotAvailable);
    OnAccountLinkingResult(AccountLinkingResult{});
    return;
  }
  GetApiClient()->InvokeInstrumentManager(
      primary_account, action_token,
      base::BindOnce(&NativeAccountLinkingHandler::OnAccountLinkingResult,
                     GetWeakPtr()));
}


void NativeAccountLinkingHandler::ShowAccountLinkingPrompt() {
  std::optional<AccountLinkingParams> params = CreateAccountLinkingParams();
  if (!params) {
    return;
  }
  is_prompt_showing_ = true;
  client()->ShowAccountLinkingPrompt(
      *params,
      base::BindOnce(&NativeAccountLinkingHandler::OnAccepted, GetWeakPtr()),
      base::BindOnce(&NativeAccountLinkingHandler::OnDeclined, GetWeakPtr()),
      base::BindOnce(&NativeAccountLinkingHandler::OnDismissed, GetWeakPtr()));
}

void NativeAccountLinkingHandler::DismissPrompt() {
  if (!is_prompt_showing_) {
    return;
  }
  is_prompt_showing_ = false;
  client_->DismissPrompt();
}

FacilitatedPaymentsApiClient* NativeAccountLinkingHandler::GetApiClient() {
  if (!api_client_ && api_client_creator_) {
    api_client_ = api_client_creator_.Run();
  }
  return api_client_.get();
}

void NativeAccountLinkingHandler::
    OnGetDetailsForCreatePaymentInstrumentResponseReceived(
        base::TimeTicks start_time,
        autofill::payments::PaymentsAutofillClient::PaymentsRpcResult
            rpc_result,
        bool is_eligible,
        const std::vector<uint8_t>& action_token) {
  base::TimeDelta latency = base::TimeTicks::Now() - start_time;
  bool result =
      rpc_result ==
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess;

  // 1. Calculate success: both the RPC must succeed and the user must be
  // eligible.
  bool is_successful = result && is_eligible;

  // 2. Log the overall success/failure result and latency.
  LogAccountLinkingGetDetailsForCreatePaymentInstrumentResultAndLatency(
      GetHistogramSuffix(), is_successful, latency);

  // 3. On success, store the action token early and trigger the UI.
  if (is_successful) {
    action_token_ = action_token;
    DoOnGetDetailsForCreatePaymentInstrumentResponse(true);
    return;
  }

  // 4. On failure, log the specific reason why the flow exited.
  if (!result) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(),
        AccountLinkingFlowExitedReason::kGetDetailsFailed);
  } else {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(),
        AccountLinkingFlowExitedReason::kNotEligiblePerPaymentsBackend);
  }

  // 5. Notify the UI to tear down the loading states first.
  auto weak_this = GetWeakPtr();
  DoOnGetDetailsForCreatePaymentInstrumentResponse(false);

  if (!weak_this) {
    return;
  }

  // 6. Return the empty result to the caller.
  OnAccountLinkingResult(AccountLinkingResult{});
}

void NativeAccountLinkingHandler::OnAccepted() {
  if (auto* strike_db = GetStrikeDatabase()) {
    strike_db->ClearStrikes();
  }
  DoOnAccepted();
  DismissPrompt();
  if (action_token_.empty()) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(),
        AccountLinkingFlowExitedReason::kActionTokenNotAvailable);
    OnAccountLinkingResult(AccountLinkingResult{});
    return;
  }
  std::optional<CoreAccountInfo> account_info = client_->GetCoreAccountInfo();
  if (!account_info.has_value() || account_info.value().IsEmpty()) {
    LogAccountLinkingFlowExitedReason(
        GetHistogramSuffix(), AccountLinkingFlowExitedReason::kUserLoggedOut);
    OnAccountLinkingResult(AccountLinkingResult{});
    return;
  }
  InvokeInstrumentManager(account_info.value(), action_token_);
}

void NativeAccountLinkingHandler::OnDeclined() {
  if (auto* strike_db = GetStrikeDatabase()) {
    strike_db->AddStrike();
  }
  LogAccountLinkingFlowExitedReason(
      GetHistogramSuffix(), AccountLinkingFlowExitedReason::kUserDeclined);
  DismissPrompt();
  OnAccountLinkingResult(AccountLinkingResult{});
}

void NativeAccountLinkingHandler::OnDismissed() {
  LogAccountLinkingFlowExitedReason(
      GetHistogramSuffix(),
      AccountLinkingFlowExitedReason::kScreenClosedByUser);
  DismissPrompt();
  OnAccountLinkingResult(AccountLinkingResult{});
}

}  // namespace payments::facilitated
