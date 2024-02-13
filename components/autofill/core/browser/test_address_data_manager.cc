// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/address_data_manager.h"

#include <memory>
#include <vector>

namespace autofill {

TestAddressDataManager::TestAddressDataManager(
    base::RepeatingClosure notify_pdm_observers)
    : AddressDataManager(/*webdata_service=*/nullptr,
                         notify_pdm_observers,
                         "en-US") {}

TestAddressDataManager::~TestAddressDataManager() = default;

void TestAddressDataManager::AddProfile(const AutofillProfile& profile) {
  std::unique_ptr<AutofillProfile> profile_ptr =
      std::make_unique<AutofillProfile>(profile);
  profile_ptr->FinalizeAfterImport();
  GetProfileStorage(profile.source()).push_back(std::move(profile_ptr));
  notify_pdm_observers_.Run();
}

void TestAddressDataManager::UpdateProfile(const AutofillProfile& profile) {
  AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());
  if (existing_profile) {
    *existing_profile = profile;
    notify_pdm_observers_.Run();
  }
}

void TestAddressDataManager::RemoveProfile(const std::string& guid) {
  AutofillProfile* profile = GetProfileByGUID(guid);
  std::vector<std::unique_ptr<AutofillProfile>>& profiles =
      GetProfileStorage(profile->source());
  profiles.erase(base::ranges::find(profiles, profile,
                                    &std::unique_ptr<AutofillProfile>::get));
}

void TestAddressDataManager::LoadProfiles() {
  // Usually, this function would reload data from the database. Since the
  // TestAddressDataManager doesn't use a database, this is a no-op.
  has_initial_load_finished_ = true;
  // In the non-test AddressDataManager, stored address metrics are emitted
  // after the initial load.
}

void TestAddressDataManager::RecordUseOf(const AutofillProfile& profile) {
  if (AutofillProfile* adm_profile = GetProfileByGUID(profile.guid())) {
    adm_profile->RecordAndLogUse();
  }
}

void TestAddressDataManager::ClearProfiles() {
  GetProfileStorage(AutofillProfile::Source::kLocalOrSyncable).clear();
  GetProfileStorage(AutofillProfile::Source::kAccount).clear();
}

}  // namespace autofill
