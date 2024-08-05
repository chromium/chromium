// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_with_in_memory_cache.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace consent_auditor {

class ConsentSyncBridgeImpl : public ConsentSyncBridge,
                              public syncer::DataTypeSyncBridge {
 public:
  ConsentSyncBridgeImpl(
      syncer::OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  ConsentSyncBridgeImpl(const ConsentSyncBridgeImpl&) = delete;
  ConsentSyncBridgeImpl& operator=(const ConsentSyncBridgeImpl&) = delete;

  ~ConsentSyncBridgeImpl() override;

  // DataTypeSyncBridge implementation.
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
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  // ConsentSyncBridge implementation.
  void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  static std::string GetStorageKeyFromSpecificsForTest(
      const sync_pb::UserConsentSpecifics& specifics);
  std::unique_ptr<syncer::DataTypeStore> StealStoreForTest();

 private:
  using StoreWithCache =
      syncer::DataTypeStoreWithInMemoryCache<sync_pb::UserConsentSpecifics>;

  void RecordConsentImpl(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics);
  // Record events in the deferred queue and clear the queue.
  void ProcessQueuedEvents();

  void OnStoreLoaded(const std::optional<syncer::ModelError>& error,
                     std::unique_ptr<StoreWithCache> store,
                     std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnStoreCommit(const std::optional<syncer::ModelError>& error);

  // Resubmits all the consents persisted in the store to the processor, which
  // were preserved when sync was disabled. This may resubmit entities that the
  // processor already knows about (i.e. with metadata), but it is allowed.
  void ResubmitAllData();

  // Persistent storage for in flight consents. Should remain quite small, as
  // entries are deleted upon commit confirmation. May contain consents without
  // metadata (e.g. persisted when sync was disabled).
  // Null upon construction, until the store is successfully initialized.
  std::unique_ptr<StoreWithCache> store_;

  // Used to store consents while the store or change processor are not
  // ready.
  std::vector<std::unique_ptr<sync_pb::UserConsentSpecifics>>
      deferred_consents_while_initializing_;

  base::WeakPtrFactory<ConsentSyncBridgeImpl> weak_ptr_factory_{this};
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_
