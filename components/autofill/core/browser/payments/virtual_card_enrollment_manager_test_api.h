// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

class VirtualCardEnrollmentManagerTestApi {
 public:
  explicit VirtualCardEnrollmentManagerTestApi(
      VirtualCardEnrollmentManager& manager)
      : manager_(manager) {}
  ~VirtualCardEnrollmentManagerTestApi() = default;

  void OnDidGetUpdateVirtualCardEnrollmentResponse(
      VirtualCardEnrollmentRequestType type,
      payments::PaymentsAutofillClient::PaymentsRpcResult result) {
    manager_->OnDidGetUpdateVirtualCardEnrollmentResponse(type, result);
  }

  void Reset() { manager_->Reset(); }

  const VirtualCardEnrollmentProcessState& state() { return manager_->state_; }

  bool ShouldContinueExistingDownstreamEnrollment(
      const CreditCard& credit_card,
      VirtualCardEnrollmentSource virtual_card_enrollment_source) {
    return manager_->ShouldContinueExistingDownstreamEnrollment(
        credit_card, virtual_card_enrollment_source);
  }

  void OnRiskDataLoadedForVirtualCard(const std::string& risk_data) {
    manager_->OnRiskDataLoadedForVirtualCard(risk_data);
  }

  void OnDidGetDetailsForEnrollResponse(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::GetDetailsForEnrollmentResponseDetails& response) {
    manager_->OnDidGetDetailsForEnrollResponse(result, response);
  }

  VirtualCardEnrollmentStrikeDatabase*
  GetVirtualCardEnrollmentStrikeDatabase() {
    return manager_->GetVirtualCardEnrollmentStrikeDatabase();
  }

 private:
  const raw_ref<VirtualCardEnrollmentManager> manager_;
};

inline VirtualCardEnrollmentManagerTestApi test_api(
    VirtualCardEnrollmentManager& manager) {
  return VirtualCardEnrollmentManagerTestApi(manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_TEST_API_H_
