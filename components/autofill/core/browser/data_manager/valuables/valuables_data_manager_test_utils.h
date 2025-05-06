// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"

namespace autofill {

// Helper class to wait for an `OnValuablesDataChanged()` call from the `vdm`.
// This is necessary since ValuablesDataManager operates asynchronously on the
// WebDatabase. Example usage:
//   ValuablesDataChangedWaiter(&vdm).Wait();
class ValuablesDataChangedWaiter : public ValuablesDataManager::Observer {
 public:
  explicit ValuablesDataChangedWaiter(ValuablesDataManager* vdm);
  ~ValuablesDataChangedWaiter() override;

  // Waits for `OnValuablesDataChanged()` to trigger.
  void Wait(const base::Location& location = FROM_HERE);

  // ValuablesDataManager::Observer:
  void OnValuablesDataChanged() override;

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ValuablesDataManager, ValuablesDataManager::Observer>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_TEST_UTILS_H_
