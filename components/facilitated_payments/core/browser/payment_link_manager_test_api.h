// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_MANAGER_TEST_API_H_

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"

namespace payments::facilitated {

class PaymentLinkManagerTestApi {
 public:
  explicit PaymentLinkManagerTestApi(PaymentLinkManager* payment_link_manager)
      : payment_link_manager_(CHECK_DEREF(payment_link_manager)) {}
  PaymentLinkManagerTestApi(const PaymentLinkManagerTestApi&) = delete;
  PaymentLinkManagerTestApi& operator=(const PaymentLinkManagerTestApi&) =
      delete;
  ~PaymentLinkManagerTestApi() = default;

  void set_initiate_payment_request_details(
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
          initiate_payment_request_details) {
    payment_link_manager_->initiate_payment_request_details_ =
        std::move(initiate_payment_request_details);
  }

  void set_scheme(PaymentLinkValidator::Scheme scheme) {
    payment_link_manager_->scheme_ = scheme;
  }

  void set_is_device_bound(bool is_device_bound) {
    payment_link_manager_->is_device_bound_for_logging_ = is_device_bound;
  }

  void set_is_payment_app_available(bool is_payment_app_available) {
    payment_link_manager_->is_payment_app_available_ = is_payment_app_available;
  }

  bool is_device_bound() {
    return payment_link_manager_->is_device_bound_for_logging_;
  }

  FacilitatedPaymentsApiClient* GetApiClient() {
    return payment_link_manager_->GetApiClient();
  }

  void OnEwalletAccountSelected(int64_t selected_instrument_id) {
    payment_link_manager_->OnEwalletAccountSelected(selected_instrument_id);
  }

  void OnPaymentAppSelected(std::string_view package_name,
                            std::string_view activity_name) {
    payment_link_manager_->OnPaymentAppSelected(package_name, activity_name);
  }

  void OnRiskDataLoaded(base::TimeTicks start_time,
                        const std::string& risk_data) {
    payment_link_manager_->OnRiskDataLoaded(start_time, risk_data);
  }

  void OnGetClientToken(base::TimeTicks start_time,
                        std::vector<uint8_t> client_token) {
    payment_link_manager_->OnGetClientToken(start_time, client_token);
  }

  FacilitatedPaymentsInitiatePaymentRequestDetails*
  initiate_payment_request_details() {
    return payment_link_manager_->initiate_payment_request_details_.get();
  }

  void SendInitiatePaymentRequest() {
    payment_link_manager_->SendInitiatePaymentRequest();
  }

  void OnInitiatePaymentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details) {
    payment_link_manager_->OnInitiatePaymentResponseReceived(
        start_time, result, std::move(response_details));
  }

  UiState ui_state() { return payment_link_manager_->ui_state_; }

  void OnUiScreenEvent(UiEvent ui_event_type) {
    payment_link_manager_->OnUiScreenEvent(ui_event_type);
  }

  void ShowPaymentLinkPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      std::unique_ptr<FacilitatedPaymentsAppInfoList> app_suggestions,
      base::OnceCallback<void(SelectedFopData)> on_fop_selected) {
    payment_link_manager_->ShowPaymentLinkPrompt(ewallet_suggestions,
                                                 std::move(app_suggestions),
                                                 std::move(on_fop_selected));
  }

  void ShowProgressScreen() { payment_link_manager_->ShowProgressScreen(); }

  void ShowErrorScreen() { payment_link_manager_->ShowErrorScreen(); }

  void OnTransactionResult(base::TimeTicks start_time,
                           PurchaseActionResult result) {
    payment_link_manager_->OnTransactionResult(start_time, result);
  }

 private:
  const raw_ref<PaymentLinkManager> payment_link_manager_;
};

inline PaymentLinkManagerTestApi test_api(PaymentLinkManager& manager) {
  return PaymentLinkManagerTestApi(&manager);
}

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_MANAGER_TEST_API_H_
