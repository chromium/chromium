// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/mock_virtual_card_enrollment_manager.h"

#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

MockVirtualCardEnrollmentManager::MockVirtualCardEnrollmentManager(
    PaymentsDataManager* payments_data_manager,
    PaymentsNetworkInterfaceVariation payments_network_interface,
    TestAutofillClient* autofill_client)
    : TestVirtualCardEnrollmentManager(payments_data_manager,
                                       payments_network_interface,
                                       autofill_client) {}

MockVirtualCardEnrollmentManager::~MockVirtualCardEnrollmentManager() = default;

}  // namespace autofill
