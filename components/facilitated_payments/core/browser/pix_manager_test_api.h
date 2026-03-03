// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_MANAGER_TEST_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/facilitated_payments/core/mojom/pix_code_validator.mojom.h"

namespace payments::facilitated {

class PixManagerTestApi {
 public:
  explicit PixManagerTestApi(PixManager& manager) : manager_(manager) {}
  ~PixManagerTestApi() = default;

  void OnPixCodeValidated(
      std::optional<PixCodeRustValidationResult> rust_validation_result,
      std::string pix_code,
      base::TimeTicks start_time,
      base::expected<mojom::PixQrCodeType, std::string> pix_qr_code_type) {
    manager_->OnPixCodeValidated(rust_validation_result, std::move(pix_code),
                                 start_time, std::move(pix_qr_code_type));
  }

  void OnApiAvailabilityReceived(base::TimeTicks start_time,
                                 bool is_api_available) {
    manager_->OnApiAvailabilityReceived(start_time, is_api_available);
  }

  void OnPixAccountSelected(base::TimeTicks fop_selector_shown_timestamp,
                            int64_t selected_instrument_id) {
    manager_->OnPixAccountSelected(fop_selector_shown_timestamp,
                                   selected_instrument_id);
  }

  void OnRiskDataLoaded(base::TimeTicks start_time,
                        const std::string& risk_data) {
    manager_->OnRiskDataLoaded(start_time, risk_data);
  }

  void OnGetClientToken(base::TimeTicks start_time,
                        std::vector<uint8_t> client_token) {
    manager_->OnGetClientToken(start_time, std::move(client_token));
  }

  void OnInitiatePaymentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details) {
    manager_->OnInitiatePaymentResponseReceived(start_time, result,
                                                std::move(response_details));
  }

  void OnPurchaseActionResult(base::TimeTicks start_time,
                              PurchaseActionResult result) {
    manager_->OnPurchaseActionResult(start_time, result);
  }

  void OnUiScreenEvent(UiEvent ui_event_type) {
    manager_->OnUiScreenEvent(ui_event_type);
  }

  void SendInitiatePaymentRequest() { manager_->SendInitiatePaymentRequest(); }

  void ShowPixPaymentPrompt(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(int64_t)> on_pix_account_selected) {
    manager_->ShowPixPaymentPrompt(bank_account_suggestions,
                                   std::move(on_pix_account_selected));
  }

  void ShowProgressScreen() { manager_->ShowProgressScreen(); }
  void ShowErrorScreen() { manager_->ShowErrorScreen(); }
  void DismissPrompt() { manager_->DismissPrompt(); }

  FacilitatedPaymentsApiClient* api_client() {
    return manager_->api_client_.get();
  }

  FacilitatedPaymentsApiClientCreator& api_client_creator() {
    return manager_->api_client_creator_;
  }

  UiState ui_state() { return manager_->ui_state_; }

  FacilitatedPaymentsInitiatePaymentRequestDetails*
  initiate_payment_request_details() {
    return manager_->initiate_payment_request_details_.get();
  }

  bool pix_code_is_in_iframe() { return manager_->pix_code_is_in_iframe_; }

  bool HasWeakPtrs() const { return manager_->weak_ptr_factory_.HasWeakPtrs(); }

  FacilitatedPaymentsApiClient* GetApiClient() {
    return manager_->GetApiClient();
  }

 private:
  const raw_ref<PixManager> manager_;
};

inline PixManagerTestApi test_api(PixManager& manager) {
  return PixManagerTestApi(manager);
}

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_MANAGER_TEST_API_H_
