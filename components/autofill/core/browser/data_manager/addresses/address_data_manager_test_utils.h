// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ADDRESS_DATA_MANAGER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ADDRESS_DATA_MANAGER_TEST_UTILS_H_

#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"

namespace autofill {

// Helper class to wait for an `OnAddressDataChanged()` call from the `adm`.
// This is necessary since ADM operates asynchronously on the WebDatabase.
// Example usage:
//   adm.AddProfile(AutofillProfile());
//   AddressDataManagerWaiter(&adm).Wait();
class AddressDataChangedWaiter : public AddressDataManager::Observer {
 public:
  explicit AddressDataChangedWaiter(AddressDataManager* adm);
  ~AddressDataChangedWaiter() override;

  // Waits for `OnAddressDataChanged()` to trigger.
  void Wait(const base::Location& location = FROM_HERE) &&;

  // AddressDataManager::Observer:
  void OnAddressDataChanged() override;

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ADDRESS_DATA_MANAGER_TEST_UTILS_H_
