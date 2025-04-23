// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace autofill {

class ValuablesDataManagerTestApi {
 public:
  explicit ValuablesDataManagerTestApi(
      ValuablesDataManager* valuables_data_manager)
      : valuables_data_manager_(*valuables_data_manager) {}

  void LoadLoyaltyCards() { valuables_data_manager_->LoadLoyaltyCards(); }

 private:
  const raw_ref<ValuablesDataManager> valuables_data_manager_;
};

inline ValuablesDataManagerTestApi test_api(
    ValuablesDataManager& valuables_data_manager) {
  return ValuablesDataManagerTestApi(&valuables_data_manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_TEST_API_H_
