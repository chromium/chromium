// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_MANDATORY_REAUTH_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_MANDATORY_REAUTH_MANAGER_H_

#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill::payments {

class MockMandatoryReauthManager : public MandatoryReauthManager {
 public:
  MockMandatoryReauthManager();
  ~MockMandatoryReauthManager() override;

  MOCK_METHOD(bool,
              ShouldOfferOptin,
              (std::optional<NonInteractivePaymentMethodType>),
              (override));
  MOCK_METHOD(void, StartOptInFlow, (), (override));
  MOCK_METHOD(void, OnUserAcceptedOptInPrompt, (), (override));
  MOCK_METHOD(void, OnOptInAuthenticationStepCompleted, (bool), (override));
  MOCK_METHOD(void, OnUserCancelledOptInPrompt, (), (override));
  MOCK_METHOD(void, OnUserClosedOptInPrompt, (), (override));
  MOCK_METHOD(
      void,
      Authenticate,
      ((device_reauth::DeviceAuthenticator::AuthenticateCallback callback)),
      (override));
  MOCK_METHOD(
      void,
      AuthenticateWithMessage,
      ((const std::u16string& message),
       (device_reauth::DeviceAuthenticator::AuthenticateCallback callback)),
      (override));
  MOCK_METHOD(void,
              StartDeviceAuthentication,
              (NonInteractivePaymentMethodType type,
               base::OnceCallback<void(bool)> authentication_complete_callback),
              (override));
  MOCK_METHOD(MandatoryReauthAuthenticationMethod,
              GetAuthenticationMethod,
              (),
              (override));
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_MANDATORY_REAUTH_MANAGER_H_
