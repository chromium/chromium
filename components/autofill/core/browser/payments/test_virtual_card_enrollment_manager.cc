// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"

namespace autofill {

TestVirtualCardEnrollmentManager::TestVirtualCardEnrollmentManager(
    raw_ptr<TestAutofillClient> autofill_client,
    raw_ptr<TestPersonalDataManager> personal_data_manager)
    : VirtualCardEnrollmentManager(autofill_client, personal_data_manager) {}

TestVirtualCardEnrollmentManager::~TestVirtualCardEnrollmentManager() = default;

void TestVirtualCardEnrollmentManager::
    OnDidGetUpdateVirtualCardEnrollmentResponse(
        AutofillClient::PaymentsRpcResult result) {
  result_ = result;
}

void TestVirtualCardEnrollmentManager::Reset() {
  reset_called_ = true;
}

void TestVirtualCardEnrollmentManager::ShowVirtualCardEnrollmentBubble() {
  bubble_shown_ = true;
}

}  // namespace autofill
