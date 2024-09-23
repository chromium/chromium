// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"

#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"

namespace autofill {

TestVirtualCardEnrollmentManager::TestVirtualCardEnrollmentManager(
    TestPersonalDataManager* personal_data_manager,
    payments::TestPaymentsNetworkInterface* payments_network_interface,
    TestAutofillClient* autofill_client = nullptr)
    : VirtualCardEnrollmentManager(personal_data_manager,
                                   payments_network_interface,
                                   autofill_client) {}

TestVirtualCardEnrollmentManager::~TestVirtualCardEnrollmentManager() = default;

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
}

void TestVirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble() {
  bubble_shown_ = true;
  VirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble();
}

void TestVirtualCardEnrollmentManager::
    OnVirtualCardEnrollmentBubbleCancelled() {
  VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled();
}

}  // namespace autofill
