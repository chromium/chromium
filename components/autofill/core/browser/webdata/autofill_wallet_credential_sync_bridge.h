// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_CREDENTIAL_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_CREDENTIAL_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace autofill {

class AutofillWebDataBackend;
class AutofillWebDataService;

// Sync bridge responsible for applying changes of autofill wallet
// credential data between the local database and the Chrome sync server.
class AutofillWalletCredentialSyncBridge : public base::SupportsUserData::Data,
                                           public syncer::ModelTypeSyncBridge {
 public:
  // Factory method that hides dealing with change_processor and also stores the
  // created bridge within `web_data_service`. This method should only be
  // called on `web_data_service`'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataBackend* webdata_backend,
      AutofillWebDataService* web_data_service);

  static AutofillWalletCredentialSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  explicit AutofillWalletCredentialSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      AutofillWebDataBackend* web_data_backend);

  AutofillWalletCredentialSyncBridge(
      const AutofillWalletCredentialSyncBridge&) = delete;
  AutofillWalletCredentialSyncBridge& operator=(
      const AutofillWalletCredentialSyncBridge&) = delete;

  ~AutofillWalletCredentialSyncBridge() override;

  // ModelTypeSyncBridge
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

 private:
  // AutofillWalletCredentialDataSyncBridge is owned by `web_data_backend_`
  // through SupportsUserData, so it's guaranteed to outlive `this`.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_CREDENTIAL_SYNC_BRIDGE_H_
