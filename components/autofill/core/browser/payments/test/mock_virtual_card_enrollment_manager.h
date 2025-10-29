// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_

#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockVirtualCardEnrollmentManager
    : public TestVirtualCardEnrollmentManager {
 public:
  MockVirtualCardEnrollmentManager(
      PaymentsDataManager* payments_data_manager,
      PaymentsNetworkInterfaceVariation payments_network_interface,
      TestAutofillClient* autofill_client);
  ~MockVirtualCardEnrollmentManager() override;

  MOCK_METHOD(
      void,
      InitVirtualCardEnroll,
      (const CreditCard& credit_card,
       VirtualCardEnrollmentSource virtual_card_enrollment_source,
       VirtualCardEnrollmentManager::VirtualCardEnrollmentFieldsLoadedCallback
           virtual_card_enrollment_fields_loaded_callback,
       std::optional<payments::GetDetailsForEnrollmentResponseDetails>
           get_details_for_enrollment_response_details,
       PrefService* user_prefs,
       VirtualCardEnrollmentManager::RiskAssessmentFunction
           risk_assessment_function),
      (override));

  MOCK_METHOD(bool,
              ShouldContinueExistingDownstreamEnrollment,
              (const CreditCard& credit_card,
               VirtualCardEnrollmentSource virtual_card_enrollment_source),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
