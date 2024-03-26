// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_PAYMENTS_WINDOW_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_PAYMENTS_WINDOW_MANAGER_H_

#include "components/autofill/core/browser/payments/payments_window_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill::payments {

class MockPaymentsWindowManager : public PaymentsWindowManager {
 public:
  MockPaymentsWindowManager();
  ~MockPaymentsWindowManager() override;

  MOCK_METHOD(void, InitVcn3dsAuthentication, (Vcn3dsContext), (override));
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_PAYMENTS_WINDOW_MANAGER_H_
