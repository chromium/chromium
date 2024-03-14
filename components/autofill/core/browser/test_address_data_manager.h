// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_

#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/strike_databases/test_inmemory_strike_database.h"

namespace autofill {

// An AddressDataManager that doesn't communicate with a database and is thus
// fully synchronous.
class TestAddressDataManager : public AddressDataManager {
 public:
  explicit TestAddressDataManager(base::RepeatingClosure notify_pdm_observers);
  ~TestAddressDataManager() override;

  using AddressDataManager::GetProfileMigrationStrikeDatabase;
  using AddressDataManager::GetProfileSaveStrikeDatabase;
  using AddressDataManager::GetProfileUpdateStrikeDatabase;
  using AddressDataManager::SetPrefService;

  // AddressDataManager overrides:
  void AddProfile(const AutofillProfile& profile) override;
  void UpdateProfile(const AutofillProfile& profile) override;
  void RemoveProfile(const std::string& guid) override;
  void LoadProfiles() override;
  void RecordUseOf(const AutofillProfile& profile) override;
  bool IsAutofillProfileEnabled() const override;

  void ClearProfiles();

  void SetAutofillProfileEnabled(bool autofill_profile_enabled) {
    autofill_profile_enabled_ = autofill_profile_enabled;
  }

 private:
  std::optional<bool> autofill_profile_enabled_;
  TestInMemoryStrikeDatabase inmemory_strike_database_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_
