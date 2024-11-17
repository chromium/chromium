// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_local_data_batch_uploader.h"

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/profile_requirement_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/local_data_description.h"

namespace autofill {

ContactInfoLocalDataBatchUploader::ContactInfoLocalDataBatchUploader(
    base::RepeatingCallback<AddressDataManager*()>
        address_data_manager_callback)
    : address_data_manager_callback_(address_data_manager_callback) {}

ContactInfoLocalDataBatchUploader::~ContactInfoLocalDataBatchUploader() =
    default;

AddressDataManager& ContactInfoLocalDataBatchUploader::GetAddressDataManager() {
  return CHECK_DEREF(address_data_manager_callback_.Run());
}

std::vector<const AutofillProfile*> ContactInfoLocalDataBatchUploader::
    GetAllLocalAutofillProfilesEligibleForMigration() {
  const AddressDataManager& address_data_manager = GetAddressDataManager();
  if (!address_data_manager.IsEligibleForAddressAccountStorage()) {
    return {};
  }

  // Get all the local addresses ordered by their ranking score (combination of
  // usage count and recency).
  std::vector<const AutofillProfile*> local_profiles =
      address_data_manager.GetProfilesByRecordType(
          AutofillProfile::RecordType::kLocalOrSyncable,
          AddressDataManager::ProfileOrder::kHighestFrecencyDesc);
  // Only consider profiles that are eligible to be moved to the account
  // storage.
  std::erase_if(local_profiles, [&](const AutofillProfile* profile) {
    return !IsMinimumAddress(*profile) ||
           !address_data_manager.IsCountryEligibleForAccountStorage(
               base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY)));
  });

  return local_profiles;
}

void ContactInfoLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  std::vector<const AutofillProfile*> local_profiles =
      GetAllLocalAutofillProfilesEligibleForMigration();

  syncer::LocalDataDescription local_data;
  local_data.type = syncer::DataType::CONTACT_INFO;
  const std::string& app_locale = GetAddressDataManager().app_locale();
  for (const AutofillProfile* profile : local_profiles) {
    syncer::LocalDataItemModel item;
    item.id = profile->guid();
    item.title = base::UTF16ToUTF8(profile->GetInfo(NAME_FULL, app_locale));
    item.subtitle =
        base::UTF16ToUTF8(profile->GetInfo(ADDRESS_HOME_LINE1, app_locale));
    // `item.icon_url` is left empty on purpose so that no icon is shown.

    local_data.local_data_models.push_back(std::move(item));
  }

  std::move(callback).Run(std::move(local_data));
}

void ContactInfoLocalDataBatchUploader::TriggerLocalDataMigration(
    const std::vector<const AutofillProfile*>& local_profiles_to_migrate) {
  AddressDataManager& address_data_manager = GetAddressDataManager();
  for (const AutofillProfile* profile : local_profiles_to_migrate) {
    // TODO(crbug.com/372223673): Resolve duplication of addresses; we should
    // not upload addresses that already exist in the account storage.
    address_data_manager.MigrateProfileToAccount(*profile);
  }
}

void ContactInfoLocalDataBatchUploader::TriggerLocalDataMigration() {
  TriggerLocalDataMigration(GetAllLocalAutofillProfilesEligibleForMigration());
}

void ContactInfoLocalDataBatchUploader::TriggerLocalDataMigration(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  // Read `syncer::LocalDataItemModel::DataId` as `std::string` as the Id type
  // used. The set of `std::string_view` is used to efficiently search in the
  // list of local profiles.
  std::set<std::string_view> guids_to_migrate;
  for (const syncer::LocalDataItemModel::DataId& item_id : items) {
    guids_to_migrate.insert(std::get<std::string>(item_id));
  }

  std::vector<const AutofillProfile*> local_profiles =
      GetAllLocalAutofillProfilesEligibleForMigration();
  std::erase_if(local_profiles, [&guids_to_migrate](const AutofillProfile* p) {
    return !guids_to_migrate.contains(p->guid());
  });

  TriggerLocalDataMigration(local_profiles);
}

}  // namespace autofill
