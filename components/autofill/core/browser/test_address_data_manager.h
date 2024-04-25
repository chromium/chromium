// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_

#include <memory>

#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/strike_databases/test_inmemory_strike_database.h"

namespace autofill {

// An AddressDataManager that doesn't communicate with a database and is thus
// fully synchronous.
class TestAddressDataManager : public AddressDataManager {
 public:
  explicit TestAddressDataManager(const std::string& app_locale = "en-US");
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
  AddressCountryCode GetDefaultCountryCodeForNewAddress() const override;
  bool IsAutofillProfileEnabled() const override;
  bool IsEligibleForAddressAccountStorage() const override;

  void ClearProfiles();

  void SetDefaultCountryCode(AddressCountryCode default_country_code) {
    default_country_code_ = std::move(default_country_code);
  }

  void SetVariationCountryCode(GeoIpCountryCode country_code) {
    variation_country_code_ = std::move(country_code);
  }

  void SetAutofillProfileEnabled(bool autofill_profile_enabled) {
    autofill_profile_enabled_ = autofill_profile_enabled;
  }

  void SetIsEligibleForAddressAccountStorage(bool eligible) {
    eligible_for_account_storage_ = eligible;
  }

 private:
  std::optional<AddressCountryCode> default_country_code_;
  std::optional<bool> autofill_profile_enabled_;
  std::optional<bool> eligible_for_account_storage_;
  TestInMemoryStrikeDatabase inmemory_strike_database_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_DATA_MANAGER_H_
