// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_TEST_VALUABLES_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_TEST_VALUABLES_DATA_MANAGER_H_

#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"

namespace autofill {

// A simplistic ValuablesDataManager used for testing.
class TestValuablesDataManager : public ValuablesDataManager {
 public:
  TestValuablesDataManager();
  TestValuablesDataManager(const TestValuablesDataManager&) = delete;
  TestValuablesDataManager& operator=(const TestValuablesDataManager&) = delete;
  ~TestValuablesDataManager() override;

  base::span<const LoyaltyCard> GetLoyaltyCards() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_TEST_VALUABLES_DATA_MANAGER_H_
