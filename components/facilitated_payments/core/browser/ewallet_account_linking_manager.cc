// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager.h"

#include "base/logging.h"
#include "components/facilitated_payments/core/browser/account_linking_params.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

namespace payments::facilitated {

EwalletAccountLinkingManager::EwalletAccountLinkingManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator,
    const autofill::Ewallet& ewallet_creation_option)
    : NativeAccountLinkingHandler(client, std::move(api_client_creator)),
      ewallet_creation_option_(ewallet_creation_option) {}

EwalletAccountLinkingManager::~EwalletAccountLinkingManager() = default;

void EwalletAccountLinkingManager::TriggerAccountLinking() {
  FetchClientToken();
}

void EwalletAccountLinkingManager::DismissAndCancel() {
  if (is_prompt_showing_) {
    DismissPrompt();
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EwalletAccountLinkingManager::DoOnClientTokenReceived(
    const std::vector<uint8_t>& client_token) {
  InitiateAccountLinkingNetworkCall(client_token);
}

void EwalletAccountLinkingManager::
    DoOnGetDetailsForCreatePaymentInstrumentResponse(bool is_eligible) {
  // TODO(crbug.com/520063014): Implement preference checks and strike database
  // checks mirroring Pix.
  if (is_eligible) {
    ShowAccountLinkingPrompt();
  }
}

std::optional<AccountLinkingParams>
EwalletAccountLinkingManager::CreateAccountLinkingParams() {
  AccountLinkingParams params(FacilitatedPaymentsType::kEwallet);
  params.fop_display_name = ewallet_creation_option_.ewallet_name();
  // TODO(b/509694036): Plumb strike count when supported.
  params.strike_count = 0;
  return params;
}

void EwalletAccountLinkingManager::DoOnAccountLinkingResult(
    AccountLinkingResult result) {
  DVLOG(1) << "Ewallet account linking result: " << result.is_successful;
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
