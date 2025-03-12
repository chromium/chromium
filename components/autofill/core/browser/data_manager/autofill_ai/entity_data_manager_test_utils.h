// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_DATA_MANAGER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_DATA_MANAGER_TEST_UTILS_H_

#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

namespace autofill {

// Helper class to wait for an `OnEntityInstancesChanged()` call from the `edm`.
// This is necessary since EDM operates asynchronously on the WebDatabase.
// Example usage:
//   edm.AddOrUpdateEntityInstance(...);
//   EntityDataChangedWaiter(&adm).Wait();
class EntityDataChangedWaiter : public EntityDataManager::Observer {
 public:
  explicit EntityDataChangedWaiter(EntityDataManager* edm);
  ~EntityDataChangedWaiter() override;

  // Waits for `OnEntityInstancesChanged()` to trigger.
  void Wait(const base::Location& location = FROM_HERE) &&;

  // AddressDataManager::Observer:
  void OnEntityInstancesChanged() override;

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<EntityDataManager, EntityDataManager::Observer>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_DATA_MANAGER_TEST_UTILS_H_
