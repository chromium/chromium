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

void NativeAccountLinkingHandler::FetchClientToken() {
  if (!GetApiClient()) {
    OnAccountLinkingResult(false);
    return;
  }
  GetApiClient()->GetClientToken(
      base::BindOnce(&NativeAccountLinkingHandler::OnClientTokenReceived,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
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
    OnAccountLinkingResult(false);
    return;
  }

  DoOnClientTokenReceived(client_token);
}

void NativeAccountLinkingHandler::OnAccountLinkingResult(bool result) {
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
    OnAccountLinkingResult(false);
    return;
  }

  auto billing_customer_id = autofill::payments::GetBillingCustomerId(
      CHECK_DEREF(client_->GetPaymentsDataManager()));

  // TODO(b:505507305): Pass `client_token` to
  // `GetDetailsForCreatePaymentInstrument` once the method signature is updated
  // to accept it.
  payments_network_interface->GetDetailsForCreatePaymentInstrument(
      billing_customer_id,
      base::BindOnce(&NativeAccountLinkingHandler::
                         OnGetDetailsForCreatePaymentInstrumentResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      client_->GetPaymentsDataManager()->app_locale());
}
void NativeAccountLinkingHandler::InvokeInstrumentManager(
    const std::vector<uint8_t>& action_token) {
  // TODO(b:505507305): Trigger GMSCore InstrumentManager (Bender screens)
  // once InvokeInstrumentManager JNI bridge is added.
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
        bool is_eligible) {
  base::TimeDelta latency = base::TimeTicks::Now() - start_time;
  bool result =
      rpc_result ==
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess;

  LogAccountLinkingGetDetailsForCreatePaymentInstrumentResultAndLatency(
      GetHistogramSuffix(), is_eligible && result, latency);

  if (result && is_eligible) {
    // TODO(b:505507305): Trigger native handoff to start the instrument manager
    // flow once JNI bridge is added.
  } else {
    if (!result) {
      LogAccountLinkingFlowExitedReason(
          GetHistogramSuffix(),
          AccountLinkingFlowExitedReason::kGetDetailsFailed);
    } else if (!is_eligible) {
      LogAccountLinkingFlowExitedReason(
          GetHistogramSuffix(),
          AccountLinkingFlowExitedReason::kNotEligiblePerPaymentsBackend);
    }
    OnAccountLinkingResult(false);
  }
}

}  // namespace payments::facilitated
