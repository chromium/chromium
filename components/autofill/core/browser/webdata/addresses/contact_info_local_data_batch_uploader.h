// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/functional/callback.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"

namespace autofill {

class AddressDataManager;
class AutofillProfile;

class ContactInfoLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  // The batch uploader is initialized at construction of `DataTypeController`
  // which is itself initialized at browser context startup when creating the
  // `syncer::SyncService`. Given that `AddressDataManager` depends on the
  // `syncer::SyncService` through the `PersonalDataManager`, we can only get
  // the `AddressDataManager` through a callback that retrieves the data manager
  // after all the initialization is complete.
  // `address_data_manager_callback` should not be ran in the constructor as the
  // `syncer::SyncService` initialization is not yet complete.
  explicit ContactInfoLocalDataBatchUploader(
      base::RepeatingCallback<AddressDataManager*()>
          address_data_manager_callback);

  ContactInfoLocalDataBatchUploader(const ContactInfoLocalDataBatchUploader&) =
      delete;
  ContactInfoLocalDataBatchUploader& operator=(
      const ContactInfoLocalDataBatchUploader&) = delete;

  ~ContactInfoLocalDataBatchUploader() override;

  // syncer::DataTypeLocalDataBatchUploader implementation.
  // Returns `syncer::LocalDataItemModel::DataId` as the guid string.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  // Triggers the migration to the account storage of all current local profiles
  // eligible for migration.
  void TriggerLocalDataMigration() override;
  // Triggers the migration to the account storage of all current local profiles
  // that have their guid in `items`. `syncer::LocalDataItemModel::DataId` maps
  // to the guid that is populated in the returned values of
  // `GetLocalDataDescription()`.
  void TriggerLocalDataMigration(
      std::vector<syncer::LocalDataItemModel::DataId> items) override;

 private:
  // Returns the list of local profiles that are eligible for migration to the
  // account storage. Will return an empty list if the user is not eligible to
  // migrate profiles to their account storage even if there exist some eligible
  // profiles.
  std::vector<const AutofillProfile*>
  GetAllLocalAutofillProfilesEligibleForMigration();

  // Triggers the migration to the account storage of all profiles in
  // `local_profiles_to_migrate`.
  void TriggerLocalDataMigration(
      const std::vector<const AutofillProfile*>& local_profiles_to_migrate);

  // Returns the `AddressDataManager` from the `PersonalDataManager` through a
  // `address_data_manager_callback_`.
  AddressDataManager& GetAddressDataManager();

  // Do not use these members directly. Use `GetAddressDataManager()` to use
  // the `AddressDataManager` instance.
  base::RepeatingCallback<AddressDataManager*()> address_data_manager_callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_LOCAL_DATA_BATCH_UPLOADER_H_
