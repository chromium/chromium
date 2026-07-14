// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager.h"

#include "base/logging.h"

namespace payments::facilitated {

EwalletAccountLinkingManager::EwalletAccountLinkingManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator)
    : NativeAccountLinkingHandler(client, std::move(api_client_creator)) {}

EwalletAccountLinkingManager::~EwalletAccountLinkingManager() = default;

// TODO: b/520063014 - Implement eWallet specific concrete logic.
void EwalletAccountLinkingManager::DoOnClientTokenReceived(
    const std::vector<uint8_t>& client_token) {
  InitiateAccountLinkingNetworkCall(client_token);
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

}  // namespace payments::facilitated
