// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager.h"

#include <utility>

#include "base/logging.h"
#include "components/facilitated_payments/core/browser/account_linking_params.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

namespace payments::facilitated {

EwalletAccountLinkingManager::EwalletAccountLinkingManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator,
    const autofill::Ewallet& ewallet_creation_option)
    : NativeAccountLinkingHandler(client, std::move(api_client_creator)),
      ewallet_creation_option_(ewallet_creation_option) {}

EwalletAccountLinkingManager::~EwalletAccountLinkingManager() {
  // Clear the callback so DismissAndCancel safely tears down the UI
  // without triggering a parent flow abort.
  DismissAndCancel();
}

void EwalletAccountLinkingManager::TriggerAccountLinking(
    base::OnceCallback<void(AccountLinkingResult)>
        on_account_linking_result_callback) {
  // Prevent concurrent calls by checking if a callback is already pending.
  if (!on_account_linking_result_callback_.is_null()) {
    return;
  }
  on_account_linking_result_callback_ =
      std::move(on_account_linking_result_callback);
  FetchClientToken();
}

void EwalletAccountLinkingManager::DismissAndCancel() {
  on_account_linking_result_callback_.Reset();

  // Invalidate weak pointers first to ignore any synchronous UI-dismissal
  // callbacks triggered by DismissPrompt(), preventing tear-down crashes.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (is_prompt_showing_) {
    DismissPrompt();
  }
}

void EwalletAccountLinkingManager::DoOnClientTokenReceived(
    const std::vector<uint8_t>& client_token) {
  InitiateAccountLinkingNetworkCall(client_token);
}

std::optional<AccountLinkingParams>
EwalletAccountLinkingManager::CreateAccountLinkingParams() {
  AccountLinkingParams params(FacilitatedPaymentsType::kEwallet);
  params.fop_display_name = ewallet_creation_option_.ewallet_name();
  // TODO(b/509694036): Plumb strike count when supported.
  params.strike_count = 0;
  return params;
}

strike_database::StrikeDatabaseIntegratorBase*
EwalletAccountLinkingManager::GetStrikeDatabase() {
  return nullptr;
}

bool EwalletAccountLinkingManager::IsUserPrefEnabled() const {
  return true;
}

void EwalletAccountLinkingManager::
    DoOnGetDetailsForCreatePaymentInstrumentResponse(bool is_eligible) {
  // TODO(crbug.com/520063014): Implement preference checks and strike database
  // checks mirroring Pix.
  if (is_eligible) {
    ShowAccountLinkingPrompt();
  }
}

void EwalletAccountLinkingManager::DoOnAccountLinkingResult(
    AccountLinkingResult result) {
  // Skip logging early exits to avoid artificially lowering the success rate.
  if (result.error_code != AccountLinkingResultCode::kCouldNotInvoke) {
    LogAccountLinkingResult(GetHistogramSuffix(), result.is_successful);
  }

  // Pass the result back to PaymentLinkManager to continue the checkout flow.
  if (on_account_linking_result_callback_) {
    std::move(on_account_linking_result_callback_).Run(result);
  }
}

base::DictValue EwalletAccountLinkingManager::
    GetPayloadForGetDetailsForCreatePaymentInstrument() {
  // TODO(b:505507305): Populate the payload when the generic network interface
  // supports it.
  return base::DictValue();
}

std::string_view EwalletAccountLinkingManager::GetHistogramSuffix() const {
  return "Ewallet";
}

base::WeakPtr<NativeAccountLinkingHandler>
EwalletAccountLinkingManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments::facilitated
