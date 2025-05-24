// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_

#include <memory>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"

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

  void set_scheme(PaymentLinkValidator::Scheme scheme) {
    ewallet_manager_->scheme_ = scheme;
  }

  void set_is_device_bound(bool is_device_bound) {
    ewallet_manager_->is_device_bound_for_logging_ = is_device_bound;
  }

  bool is_device_bound() {
    return ewallet_manager_->is_device_bound_for_logging_;
  }

  FacilitatedPaymentsApiClient* GetApiClient() {
    return ewallet_manager_->GetApiClient();
  }

  void OnApiAvailabilityReceived(base::TimeTicks start_time,
                                 bool is_api_available) {
    ewallet_manager_->OnApiAvailabilityReceived(start_time, is_api_available);
  }

  void OnEwalletAccountSelected(int64_t selected_instrument_id) {
    ewallet_manager_->OnEwalletAccountSelected(selected_instrument_id);
  }

  void OnRiskDataLoaded(base::TimeTicks start_time,
                        const std::string& risk_data) {
    ewallet_manager_->OnRiskDataLoaded(start_time, risk_data);
  }

  void OnGetClientToken(base::TimeTicks start_time,
                        std::vector<uint8_t> client_token) {
    ewallet_manager_->OnGetClientToken(start_time, client_token);
  }

  FacilitatedPaymentsInitiatePaymentRequestDetails*
  initiate_payment_request_details() {
    return ewallet_manager_->initiate_payment_request_details_.get();
  }

  void SendInitiatePaymentRequest() {
    ewallet_manager_->SendInitiatePaymentRequest();
  }

  void OnInitiatePaymentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details) {
    ewallet_manager_->OnInitiatePaymentResponseReceived(
        start_time, result, std::move(response_details));
  }

  UiState ui_state() { return ewallet_manager_->ui_state_; }

  void OnUiEvent(UiEvent ui_event_type) {
    ewallet_manager_->OnUiEvent(ui_event_type);
  }

  void ShowEwalletPaymentPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      base::OnceCallback<void(int64_t)> on_ewallet_account_selected) {
    ewallet_manager_->ShowEwalletPaymentPrompt(
        ewallet_suggestions, std::move(on_ewallet_account_selected));
  }

  void ShowProgressScreen() { ewallet_manager_->ShowProgressScreen(); }

  void ShowErrorScreen() { ewallet_manager_->ShowErrorScreen(); }

  void OnTransactionResult(base::TimeTicks start_time,
                           PurchaseActionResult result) {
    ewallet_manager_->OnTransactionResult(start_time, result);
  }

 private:
  const raw_ref<EwalletManager> ewallet_manager_;
};

inline EwalletManagerTestApi test_api(EwalletManager& manager) {
  return EwalletManagerTestApi(&manager);
}

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_
