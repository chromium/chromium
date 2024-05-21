// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/address_data_manager.h"

#include <memory>
#include <vector>

namespace autofill {

TestAddressDataManager::TestAddressDataManager(
    const std::string& app_locale)
    : AddressDataManager(/*webdata_service=*/nullptr,
                         /*pref_service=*/nullptr,
                         /*local_state=*/nullptr,
                         /*sync_service=*/nullptr,
                         /*identity_manager=*/nullptr,
                         /*strike_database=*/nullptr,
                         /*variation_country_code=*/GeoIpCountryCode("US"),
                         app_locale) {
  // Not initialized through the base class constructor call, since
  // `inmemory_strike_database_` is not initialized at this point.
  SetStrikeDatabase(&inmemory_strike_database_);
}

TestAddressDataManager::~TestAddressDataManager() = default;

void TestAddressDataManager::AddProfile(const AutofillProfile& profile) {
  std::unique_ptr<AutofillProfile> profile_ptr =
      std::make_unique<AutofillProfile>(profile);
  profile_ptr->FinalizeAfterImport();
  GetProfileStorage(profile.source()).push_back(std::move(profile_ptr));
  NotifyObservers();
}

void TestAddressDataManager::UpdateProfile(const AutofillProfile& profile) {
  std::vector<std::unique_ptr<AutofillProfile>>& storage =
      GetProfileStorage(profile.source());
  auto adm_profile =
      base::ranges::find(storage, profile.guid(), &AutofillProfile::guid);
  if (adm_profile != storage.end()) {
    **adm_profile = profile;
    NotifyObservers();
  }
}

void TestAddressDataManager::RemoveProfile(const std::string& guid) {
  const AutofillProfile* profile = GetProfileByGUID(guid);
  std::vector<std::unique_ptr<AutofillProfile>>& profiles =
      GetProfileStorage(profile->source());
  profiles.erase(base::ranges::find(profiles, profile,
                                    &std::unique_ptr<AutofillProfile>::get));
  NotifyObservers();
}

void TestAddressDataManager::LoadProfiles() {
  // Usually, this function would reload data from the database. Since the
  // TestAddressDataManager doesn't use a database, this is a no-op.
  has_initial_load_finished_ = true;
  // In the non-test AddressDataManager, stored address metrics are emitted
  // after the initial load.
}

void TestAddressDataManager::RecordUseOf(const AutofillProfile& profile) {
  std::vector<std::unique_ptr<AutofillProfile>>& storage =
      GetProfileStorage(profile.source());
  auto adm_profile =
      base::ranges::find(storage, profile.guid(), &AutofillProfile::guid);
  if (adm_profile != storage.end()) {
    (*adm_profile)->RecordAndLogUse();
  }
}

AddressCountryCode TestAddressDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.has_value()) {
    return default_country_code_.value();
  }
  return AddressDataManager::GetDefaultCountryCodeForNewAddress();
}

bool TestAddressDataManager::IsAutofillProfileEnabled() const {
  // Return the value of autofill_profile_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_profile_enabled_.has_value()) {
    return autofill_profile_enabled_.value();
  }
  return AddressDataManager::IsAutofillProfileEnabled();
}

bool TestAddressDataManager::IsEligibleForAddressAccountStorage() const {
  return eligible_for_account_storage_.has_value()
             ? *eligible_for_account_storage_
             : AddressDataManager::IsEligibleForAddressAccountStorage();
}

void TestAddressDataManager::ClearProfiles() {
  GetProfileStorage(AutofillProfile::Source::kLocalOrSyncable).clear();
  GetProfileStorage(AutofillProfile::Source::kAccount).clear();
}

}  // namespace autofill
