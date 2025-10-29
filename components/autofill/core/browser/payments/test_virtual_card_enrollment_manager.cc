// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"

#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {

TestVirtualCardEnrollmentManager::TestVirtualCardEnrollmentManager(
    PaymentsDataManager* payments_data_manager,
    PaymentsNetworkInterfaceVariation payments_network_interface,
    TestAutofillClient* autofill_client = nullptr)
    : VirtualCardEnrollmentManager(payments_data_manager,
                                   payments_network_interface,
                                   autofill_client) {}

TestVirtualCardEnrollmentManager::~TestVirtualCardEnrollmentManager() = default;

bool TestVirtualCardEnrollmentManager::ShouldBlockVirtualCardEnrollment(
    const std::string& instrument_id,
    VirtualCardEnrollmentSource virtual_card_enrollment_source) const {
  if (ignore_strike_database_) {
    return false;
  }
  return VirtualCardEnrollmentManager::ShouldBlockVirtualCardEnrollment(
      instrument_id, virtual_card_enrollment_source);
}

void TestVirtualCardEnrollmentManager::LoadRiskDataAndContinueFlow(
    PrefService* user_prefs,
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run("some risk data");
}

void TestVirtualCardEnrollmentManager::
    OnDidGetUpdateVirtualCardEnrollmentResponse(
        VirtualCardEnrollmentRequestType type,
        payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  result_ = result;
  VirtualCardEnrollmentManager::OnDidGetUpdateVirtualCardEnrollmentResponse(
      type, result);
}

void TestVirtualCardEnrollmentManager::Reset() {
  reset_called_ = true;
  VirtualCardEnrollmentManager::Reset();
}

void TestVirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble(
    VirtualCardEnrollmentFields* virtual_card_enrollment_fields) {
  bubble_shown_ = true;
  VirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble(
      virtual_card_enrollment_fields);
}

void TestVirtualCardEnrollmentManager::
    OnVirtualCardEnrollmentBubbleCancelled() {
  VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled();
}

}  // namespace autofill
