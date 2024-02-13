// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_

#include "components/autofill/core/browser/address_data_manager.h"

namespace autofill {

// An AddressDataManager that doesn't communicate with a database and is thus
// fully synchronous.
class TestAddressDataManager : public AddressDataManager {
 public:
  explicit TestAddressDataManager(base::RepeatingClosure notify_pdm_observers);
  ~TestAddressDataManager() override;

  // AddressDataManager overrides:
  void AddProfile(const AutofillProfile& profile) override;
  void UpdateProfile(const AutofillProfile& profile) override;
  void RemoveProfile(const std::string& guid) override;
  void LoadProfiles() override;
  void RecordUseOf(const AutofillProfile& profile) override;

  void ClearProfiles();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_
