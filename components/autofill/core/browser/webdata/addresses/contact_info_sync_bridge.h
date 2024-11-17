// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

class AutofillWebDataService;

class ContactInfoSyncBridge : public AutofillWebDataServiceObserverOnDBSequence,
                              public base::SupportsUserData::Data,
                              public syncer::DataTypeSyncBridge {
 public:
  ContactInfoSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      AutofillWebDataBackend* backend);
  ~ContactInfoSyncBridge() override;

  ContactInfoSyncBridge(const ContactInfoSyncBridge&) = delete;
  ContactInfoSyncBridge& operator=(const ContactInfoSyncBridge&) = delete;

  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataBackend* web_data_backend,
      AutofillWebDataService* web_data_service);

  static syncer::DataTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

  // AutofillWebDataServiceObserverOnDBSequence implementation.
  void AutofillProfileChanged(const AutofillProfileChange& change) override;

 private:
  const sync_pb::ContactInfoSpecifics&
  GetPossiblyTrimmedContactInfoSpecificsDataFromProcessor(
      const std::string& storage_key);

  bool SyncMetadataCacheContainsSupportedFields(
      const syncer::EntityMetadataMap& metadata_map) const;

  // Returns the `AutofillTable` associated with the `web_data_backend_`.
  AddressAutofillTable* GetAutofillTable();

  AutofillSyncMetadataTable* GetSyncMetadataStore();

  // Queries all `Source::kAccount` profiles from `GetAutofillTable()` and
  // restricts the result to profiles where `filter(guid)` is true.
  // These profiles are then converted to their `ContactInfoSpecifics`
  // representation and returned as a `syncer::MutableDataBatch`.
  // If querying the database fails, a nullptr is returned and an error raised.
  std::unique_ptr<syncer::MutableDataBatch> GetDataAndFilter(
      base::RepeatingCallback<bool(const std::string&)> filter);

  // Synchronously load sync metadata from the `AutofillTable` and pass it to
  // the processor so it can start tracking changes.
  void LoadMetadata();

  // Ensures that at most one address in the storage can be labeled as home and
  // work each. If `profile` is H/W and a different address of the same record
  // type already exists in the storage, this function downgrades it to a
  // regular one. The change is intentionally not re-uploaded, because:
  // - The logic is meant to catch inconsistencies due to failed writes, which
  //   are not reflected on the server to begin with. E.g, it can happen that an
  //   address is promoted to H/W in Chrome, but persisting it on the backend
  //   fails. Then, a different address might be promoted to H/W from outside of
  //   Chrome. Since CONTACT_INFO doesn't have a way to propagate errors back to
  //   the client, this would result in duplicate H/W addresses.
  // - It avoids a potential ping-pong.
  // Returns false if storage operations fail.
  bool EnsureUniquenessOfHomeAndWork(const AutofillProfile& profile);

  // Uploads all `pending_profile_changes_`.
  void FlushPendingAccountProfileChanges();

  // The bridge should be used on the same sequence where it has been
  // constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  // ContactInfoSyncBridge is owned by `web_data_backend_` through
  // SupportsUserData, so it's guaranteed to outlive `this`.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;

  base::ScopedObservation<AutofillWebDataBackend,
                          AutofillWebDataServiceObserverOnDBSequence>
      scoped_observation_{this};

  // Contains local changes (see `AutofillProfileChanged()`) that happen before
  // the change processor starts tracking metadata. They get uploaded once the
  // change processor is ready (see `FlushPendingProfileChanges()`).
  base::queue<AutofillProfileChange> pending_account_profile_changes_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_SYNC_BRIDGE_H_
