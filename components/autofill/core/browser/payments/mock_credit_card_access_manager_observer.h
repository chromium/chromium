// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_CREDIT_CARD_ACCESS_MANAGER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_CREDIT_CARD_ACCESS_MANAGER_OBSERVER_H_

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockCreditCardAccessManagerObserver
    : public CreditCardAccessManager::Observer {
 public:
  MockCreditCardAccessManagerObserver();
  ~MockCreditCardAccessManagerObserver() override;

  MOCK_METHOD(void,
              OnCreditCardAccessManagerDestroyed,
              (CreditCardAccessManager&),
              (override));
  MOCK_METHOD(void,
              OnCreditCardFetchStarted,
              (CreditCardAccessManager&, const CreditCard&),
              (override));
  MOCK_METHOD(void,
              OnCreditCardFetchSucceeded,
              (CreditCardAccessManager&, const CreditCard&),
              (override));
  MOCK_METHOD(void,
              OnCreditCardFetchFailed,
              (CreditCardAccessManager&, const CreditCard*),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_CREDIT_CARD_ACCESS_MANAGER_OBSERVER_H_
