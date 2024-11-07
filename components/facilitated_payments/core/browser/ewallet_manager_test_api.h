// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_

#include <memory>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"

namespace payments::facilitated {

class EwalletManagerTestApi {
 public:
  explicit EwalletManagerTestApi(EwalletManager* ewallet_manager)
      : ewallet_manager_(CHECK_DEREF(ewallet_manager)) {}
  EwalletManagerTestApi(const EwalletManagerTestApi&) = delete;
  EwalletManagerTestApi& operator=(const EwalletManagerTestApi&) = delete;
  ~EwalletManagerTestApi() = default;

  void set_initiate_payment_request_details(
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
          initiate_payment_request_details) {
    ewallet_manager_->initiate_payment_request_details_ =
        std::move(initiate_payment_request_details);
  }

  FacilitatedPaymentsApiClient* GetApiClient() {
    return ewallet_manager_->GetApiClient();
  }

  void OnApiAvailabilityReceived(bool is_api_available) {
    ewallet_manager_->OnApiAvailabilityReceived(is_api_available);
  }

  void OnEwalletPaymentPromptResult(bool is_prompt_accepted,
                                    int64_t selected_instrument_id) {
    ewallet_manager_->OnEwalletPaymentPromptResult(is_prompt_accepted,
                                                   selected_instrument_id);
  }

  void OnRiskDataLoaded(const std::string& risk_data) {
    ewallet_manager_->OnRiskDataLoaded(risk_data);
  }

  void OnGetClientToken(std::vector<uint8_t> client_token) {
    ewallet_manager_->OnGetClientToken(client_token);
  }

  FacilitatedPaymentsInitiatePaymentRequestDetails*
  initiate_payment_request_details() {
    return ewallet_manager_->initiate_payment_request_details_.get();
  }

  void SendInitiatePaymentRequest() {
    ewallet_manager_->SendInitiatePaymentRequest();
  }

  void OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details) {
    ewallet_manager_->OnInitiatePaymentResponseReceived(
        result, std::move(response_details));
  }

 private:
  const raw_ref<EwalletManager> ewallet_manager_;
};

inline EwalletManagerTestApi test_api(EwalletManager& manager) {
  return EwalletManagerTestApi(&manager);
}

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_
