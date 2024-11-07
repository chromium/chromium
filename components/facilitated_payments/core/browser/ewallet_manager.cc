// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_manager.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/util/payment_link_validator.h"
#include "url/gurl.h"

namespace payments::facilitated {

EwalletManager::EwalletManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator)
    : client_(CHECK_DEREF(client)),
      api_client_creator_(std::move(api_client_creator)) {}
EwalletManager::~EwalletManager() = default;

// TODO(crbug.com/40280186): Add tests for this method.
void EwalletManager::TriggerEwalletPushPayment(const GURL& payment_link_url,
                                               const GURL& page_url) {
  if (!PaymentLinkValidator().IsValid(payment_link_url.spec())) {
    return;
  }

  // Ewallet payment flow can't be completed in the landscape mode as the
  // Payments server doesn't support it yet.
  if (client_->IsInLandscapeMode()) {
    return;
  }

  autofill::PaymentsDataManager* payments_data_manager =
      client_->GetPaymentsDataManager();
  if (!payments_data_manager) {
    return;
  }

  base::span<const autofill::Ewallet> ewallet_accounts =
      payments_data_manager->GetEwalletAccounts();
  supported_ewallets_.reserve(ewallet_accounts.size());
  std::ranges::copy_if(
      ewallet_accounts, std::back_inserter(supported_ewallets_),
      [&payment_link_url](const autofill::Ewallet& ewallet) {
        return ewallet.SupportsPaymentLink(payment_link_url.spec());
      });

  if (supported_ewallets_.size() == 0) {
    return;
  }

  // TODO(crbug.com/40280186): check allowlist.

  if (!GetApiClient()) {
    return;
  }

  initiate_payment_request_details_ =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  initiate_payment_request_details_->merchant_payment_page_hostname_ =
      page_url.host();
  initiate_payment_request_details_->payment_link_ = payment_link_url.spec();

  GetApiClient()->IsAvailable(
      base::BindOnce(&EwalletManager::OnApiAvailabilityReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EwalletManager::Reset() {
  supported_ewallets_.clear();
  initiate_payment_request_details_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

FacilitatedPaymentsApiClient* EwalletManager::GetApiClient() {
  if (!api_client_) {
    if (api_client_creator_) {
      api_client_ = std::move(api_client_creator_).Run();
    }
  }

  return api_client_.get();
}

void EwalletManager::OnApiAvailabilityReceived(bool is_api_available) {
  if (!is_api_available) {
    return;
  }

  initiate_payment_request_details_->billing_customer_number_ =
      autofill::payments::GetBillingCustomerId(
          client_->GetPaymentsDataManager());

  client_->ShowEwalletPaymentPrompt(
      supported_ewallets_,
      base::BindOnce(&EwalletManager::OnEwalletPaymentPromptResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EwalletManager::OnEwalletPaymentPromptResult(
    bool is_prompt_accepted,
    int64_t selected_instrument_id) {
  if (!is_prompt_accepted) {
    return;
  }

  client_->ShowProgressScreen();

  initiate_payment_request_details_->instrument_id_ = selected_instrument_id;

  client_->LoadRiskData(base::BindOnce(&EwalletManager::OnRiskDataLoaded,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void EwalletManager::OnRiskDataLoaded(const std::string& risk_data) {
  if (risk_data.empty()) {
    client_->ShowErrorScreen();
    return;
  }

  initiate_payment_request_details_->risk_data_ = risk_data;

  GetApiClient()->GetClientToken(base::BindOnce(
      &EwalletManager::OnGetClientToken, weak_ptr_factory_.GetWeakPtr()));
}

void EwalletManager::OnGetClientToken(std::vector<uint8_t> client_token) {
  if (client_token.empty()) {
    client_->ShowErrorScreen();
    return;
  }
  initiate_payment_request_details_->client_token_ = std::move(client_token);

  SendInitiatePaymentRequest();
}

void EwalletManager::SendInitiatePaymentRequest() {
  FacilitatedPaymentsNetworkInterface* payments_network_interface =
      client_->GetFacilitatedPaymentsNetworkInterface();

  if (!payments_network_interface) {
    client_->ShowErrorScreen();
    return;
  }

  payments_network_interface->InitiatePayment(
      std::move(initiate_payment_request_details_),
      base::BindOnce(&EwalletManager::OnInitiatePaymentResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()),
      client_->GetPaymentsDataManager()->app_locale());
}

void EwalletManager::OnInitiatePaymentResponseReceived(
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
    std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
        response_details) {
  if (result !=
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    client_->ShowErrorScreen();
    return;
  }
  if (!response_details || response_details->action_token_.empty()) {
    client_->ShowErrorScreen();
    return;
  }
  std::optional<CoreAccountInfo> account_info = client_->GetCoreAccountInfo();
  // If the user logged out after selecting the payment method, the
  // `account_info` would be empty, and the  the payment flow should be
  // abandoned.
  if (!account_info.has_value() || account_info.value().IsEmpty()) {
    client_->ShowErrorScreen();
    return;
  }
  GetApiClient()->InvokePurchaseAction(
      account_info.value(), response_details->action_token_,
      base::BindOnce(&EwalletManager::OnTransactionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EwalletManager::OnTransactionResult(
    FacilitatedPaymentsApiClient::PurchaseActionResult result) {
  // When server responds to the purchase action, Google Play Services takes
  // over, but the dismiss of progress screen is not taken over. Calling
  // `DismissPrompt` to dismiss it manually.
  client_->DismissPrompt();
}

}  // namespace payments::facilitated
