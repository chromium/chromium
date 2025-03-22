// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_TEST_API_H_

#include <utility>

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"

namespace autofill::payments {

class BnplManagerTestApi {
 public:
  explicit BnplManagerTestApi(BnplManager* bnpl_manager)
      : bnpl_manager_(CHECK_DEREF(bnpl_manager)) {}
  BnplManagerTestApi(const BnplManagerTestApi&) = delete;
  BnplManagerTestApi& operator=(const BnplManagerTestApi&) = delete;
  ~BnplManagerTestApi() = default;

  void PopulateManagerWithUserAndBnplIssuerDetails(
      int64_t billing_customer_number,
      std::string instrument_id,
      std::string risk_data,
      std::string context_token,
      GURL redirect_url,
      BnplIssuer issuer) {
    bnpl_manager_->ongoing_flow_state_ =
        std::make_unique<BnplManager::OngoingFlowState>();
    bnpl_manager_->ongoing_flow_state_->billing_customer_number =
        std::move(billing_customer_number);
    bnpl_manager_->ongoing_flow_state_->instrument_id =
        std::move(instrument_id);
    bnpl_manager_->ongoing_flow_state_->risk_data = std::move(risk_data);
    bnpl_manager_->ongoing_flow_state_->context_token =
        std::move(context_token);
    bnpl_manager_->ongoing_flow_state_->redirect_url = std::move(redirect_url);
    bnpl_manager_->ongoing_flow_state_->issuer = std::move(issuer);
  }

  void SetOnBnplVcnFetchedCallback(
      BnplManager::OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback) {
    bnpl_manager_->ongoing_flow_state_->on_bnpl_vcn_fetched_callback =
        std::move(on_bnpl_vcn_fetched_callback);
  }

  void FetchVcnDetails(GURL url) {
    bnpl_manager_->FetchVcnDetails(std::move(url));
  }

  void OnVcnDetailsFetched(
      PaymentsAutofillClient::PaymentsRpcResult result,
      const BnplFetchVcnResponseDetails& response_details) {
    bnpl_manager_->OnVcnDetailsFetched(result, response_details);
  }

  void Reset() { bnpl_manager_->Reset(); }

  BnplManager::OngoingFlowState* GetOngoingFlowState() {
    return bnpl_manager_->ongoing_flow_state_.get();
  }

  void OnTosDialogAccepted() { bnpl_manager_->OnTosDialogAccepted(); }

  void CreateBnplPaymentInstrument() {
    bnpl_manager_->CreateBnplPaymentInstrument();
  }

  void OnRedirectUrlFetched(PaymentsAutofillClient::PaymentsRpcResult result,
                            const BnplFetchUrlResponseDetails& response) {
    bnpl_manager_->OnRedirectUrlFetched(result, response);
  }

 private:
  const raw_ref<BnplManager> bnpl_manager_;
};

inline BnplManagerTestApi test_api(BnplManager& manager) {
  return BnplManagerTestApi(&manager);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_TEST_API_H_
