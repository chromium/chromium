// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_MOCK_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_MOCK_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_

#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"

namespace autofill {

// MockAlternativeStateNameMapUpdater for testing AlternativeStateNameMap
class MockAlternativeStateNameMapUpdater
    : public AlternativeStateNameMapUpdater {
 public:
  ~MockAlternativeStateNameMapUpdater() override;
  MockAlternativeStateNameMapUpdater(base::OnceClosure callback,
                                     PrefService* local_state,
                                     AddressDataManager* address_data_manager);

  // AddressDataManager::Observer:
  void OnAddressDataChanged() override;

  base::OnceClosure callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_MOCK_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_
