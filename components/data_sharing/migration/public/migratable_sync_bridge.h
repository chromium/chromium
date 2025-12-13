// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_BRIDGE_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/data_sharing/migration/public/context_id.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

class PrefService;
namespace syncer {
class DataTypeLocalChangeProcessor;
class DataTypeStore;
}  // namespace syncer

namespace data_sharing::migration {

using GetEntitiesCallback =
    base::OnceCallback<void(std::vector<std::unique_ptr<syncer::EntityData>>)>;

// A base class for sync bridges that can participate in the data sharing
// migration framework. It encapsulates the common logic for store
// initialization and interaction with the sync processor.
class COMPONENT_EXPORT(DATA_SHARING_MIGRATION) MigratableSyncBridge
    : public syncer::DataTypeSyncBridge {
 public:
  MigratableSyncBridge(
      syncer::DataType data_type,
      syncer::OnceDataTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      PrefService* pref_service,
      bool is_shared_bridge);
  ~MigratableSyncBridge() override;

  // Disallow copy/assign.
  MigratableSyncBridge(const MigratableSyncBridge&) = delete;
  MigratableSyncBridge& operator=(const MigratableSyncBridge&) = delete;

  // Returns whether this bridge is for the "shared" data type.
  bool IsBridgeShared() const { return is_shared_bridge_; }

  // Methods for the Migration Framework

  // Gets all entities associated with the given context ID. (Async)
  virtual void GetEntitiesForContext(const ContextId& context_id,
                                     GetEntitiesCallback callback) = 0;

  // Deletes all entities associated with the given context ID.
  virtual void DeleteEntitiesForContext(const ContextId& context_id) = 0;

  // Adds a single new entity to the bridge's store.
  virtual void AddEntity(std::unique_ptr<syncer::EntityData> entity_data) = 0;

  // Removes a single entity by its ID from the bridge's store.
  virtual void RemoveEntity(const std::string& entity_id) = 0;

  // Methods for Feature-Specific Logic. Must be implemented by feature bridges.

  // Determines if the given entity's data indicates it should be in a
  // shared state.
  virtual bool IsContextIdShared(const syncer::EntityData& entity) = 0;

  // Determines if an entity with the given GUID is in the correct state.
  virtual bool IsInCorrectState(const std::string& guid) = 0;

  // syncer::DataTypeSyncBridge override
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

 protected:
  // Pure virtual methods to be implemented by the feature-specific bridge.
  virtual void OnStoreCreatedAndReady() = 0;
  virtual std::optional<syncer::ModelError> ProcessSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) = 0;

  // Accessors for derived classes.
  syncer::DataTypeStore* store() const { return store_.get(); }
  PrefService* pref_service() { return pref_service_; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  const raw_ptr<PrefService> pref_service_;
  const bool is_shared_bridge_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  base::WeakPtrFactory<MigratableSyncBridge> weak_ptr_factory_{this};
};

}  // namespace data_sharing::migration

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_BRIDGE_H_
