// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_SAVE_AND_FILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_SAVE_AND_FILL_MANAGER_H_

#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockSaveAndFillManager : public payments::SaveAndFillManager {
 public:
  MockSaveAndFillManager();
  ~MockSaveAndFillManager() override;

  MOCK_METHOD(void, OnDidAcceptCreditCardSaveAndFillSuggestion, (), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_SAVE_AND_FILL_MANAGER_H_
