// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/model_error.h"

namespace sync_pb {
class EntitySpecifics;
class UniquePosition;
}  // namespace sync_pb

namespace syncer {

class ClientTagHash;
class DeletionOrigin;
class MetadataBatch;
class MetadataChangeList;
class DataTypeSyncBridge;
struct EntityData;

// Interface used by the DataTypeSyncBridge to inform sync of local changes.
// Lives on the model sequence.
class DataTypeLocalChangeProcessor {
 public:
  DataTypeLocalChangeProcessor() = default;
  virtual ~DataTypeLocalChangeProcessor() = default;

  // Inform the processor of a new or updated entity. The `entity_data` param
  // does not need to be fully set, but it should at least have specifics and
  // non-unique name. The processor will fill in the rest if the bridge does
  // not have a reason to care. For example, if `client_tag_hash` is not set,
  // the bridge's GetClientTag() will be exercised (and must be supported).
  virtual void Put(const std::string& storage_key,
                   std::unique_ptr<EntityData> entity_data,
                   MetadataChangeList* metadata_change_list) = 0;

  // Inform the processor of a deleted entity. The call is ignored if
  // `storage_key` is unknown. `origin` allows providing fine-grained
  // information about which codepath the deletion is coming from.
  virtual void Delete(const std::string& storage_key,
                      const DeletionOrigin& origin,
                      MetadataChangeList* metadata_change_list) = 0;

  // Sets storage key for the new entity. This function only applies to
  // datatypes that can't generate storage key based on EntityData. Bridge
  // should call this function when handling
  // MergeFullSyncData/ApplyIncrementalSyncChanges to inform the processor about
  // `storage_key` of an entity identified by `entity_data`. Metadata changes
  // about new entity will be appended to `metadata_change_list`.
  virtual void UpdateStorageKey(const EntityData& entity_data,
                                const std::string& storage_key,
                                MetadataChangeList* metadata_change_list) = 0;

  // Remove entity metadata and do not track the entity. Applies to datatypes
  // that support local deletions that should not get synced up (e.g. TYPED_URL
  // does not sync up deletions of expired URLs). If the deletion should get
  // synced up, use change_processor()->Delete() instead. The call is ignored if
  // `storage_key` is unknown.
  virtual void UntrackEntityForStorageKey(const std::string& storage_key) = 0;

  // Remove entity metadata and do not track the entity, exactly like
  // UntrackEntityForStorageKey() above. This method may be called even if
  // entity does not have storage key. The call is ignored if `client_tag_hash`
  // is unknown.
  virtual void UntrackEntityForClientTagHash(
      const ClientTagHash& client_tag_hash) = 0;

  // Returns the storage keys for all tracked entities (including tombstones).
  virtual std::vector<std::string> GetAllTrackedStorageKeys() const = 0;

  // Returns true if a tracked entity has local changes. A commit may or may not
  // be in progress at this time.
  virtual bool IsEntityUnsynced(const std::string& storage_key) const = 0;

  // Returns the creation timestamp of the sync entity, or a null time if the
  // entity is not tracked.
  virtual base::Time GetEntityCreationTime(
      const std::string& storage_key) const = 0;

  // Returns the modification timestamp of the sync entity, or a null time if
  // the entity is not tracked.
  virtual base::Time GetEntityModificationTime(
      const std::string& storage_key) const = 0;

  // Pass the pointer to the processor so that the processor can notify the
  // bridge of various events; `bridge` must not be nullptr and must outlive
  // this object.
  virtual void OnModelStarting(DataTypeSyncBridge* bridge) = 0;

  // The `bridge` is expected to call this exactly once unless it encounters an
  // error. Ideally ModelReadyToSync() is called as soon as possible during
  // initialization, and must be called before invoking either Put() or
  // Delete(). The bridge needs to be able to synchronously handle
  // MergeFullSyncData() and ApplyIncrementalSyncChanges() after calling
  // ModelReadyToSync(). If an error is encountered, calling ReportError()
  // instead is sufficient.
  virtual void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) = 0;

  // Returns a boolean representing whether the processor's metadata is
  // currently tracking the data type's data. This typically becomes true after
  // ModelReadyToSync() was called (if the data type is enabled). If false,
  // then Put() and Delete() will no-op and can be omitted by bridge.
  virtual bool IsTrackingMetadata() const = 0;

  // Returns the account ID for which metadata is being tracked, or empty if not
  // tracking metadata.
  virtual std::string TrackedAccountId() const = 0;

  // Returns the cache guid for which metadata is being tracked, or empty if not
  // tracking metadata.
  virtual std::string TrackedCacheGuid() const = 0;

  // Report an error in the model to sync. Should be called for any persistence
  // or consistency error the bridge encounters outside of a method that allows
  // returning a ModelError directly. Outstanding callbacks are not expected to
  // be called after an error. This will result in sync being temporarily
  // disabled for the data type (generally until the next restart).
  virtual void ReportError(const ModelError& error) = 0;

  // Returns whether the processor has encountered any error, either reported
  // by the bridge via ReportError() or by other means.
  virtual std::optional<ModelError> GetError() const = 0;

  // Returns the delegate for the controller.
  virtual base::WeakPtr<DataTypeControllerDelegate> GetControllerDelegate() = 0;

  // Returns the cached version of remote entity specifics for `storage_key` if
  // available. These specifics can be fully or partially trimmed (proto fields
  // cleared) according to the bridge's logic in
  // TrimAllSupportedFieldsFromRemoteSpecifics().
  // By default, empty EntitySpecifics is returned if the storage key is
  // unknown, or the storage key is known but trimmed specifics is not
  // available.
  virtual const sync_pb::EntitySpecifics& GetPossiblyTrimmedRemoteSpecifics(
      const std::string& storage_key) const = 0;

  // Helper methods to generate unique positions in the bridges for datatypes
  // that need a notion of ordering across entities and use
  // sync_pb.UniquePosition (recommended) to achieve so. The following methods
  // are used to generate UniquePosition relatively to the given entities
  // (`storage_key_before` and / or `storage_key_after`) for the entity
  // `target_client_tag_hash`. Note that `storage_key_before` and
  // `storage_key_after` must be already tracked by the processor and be in the
  // correct order.
  virtual sync_pb::UniquePosition UniquePositionAfter(
      const std::string& storage_key_before,
      const ClientTagHash& target_client_tag_hash) const = 0;
  virtual sync_pb::UniquePosition UniquePositionBefore(
      const std::string& storage_key_after,
      const ClientTagHash& target_client_tag_hash) const = 0;
  virtual sync_pb::UniquePosition UniquePositionBetween(
      const std::string& storage_key_before,
      const std::string& storage_key_after,
      const ClientTagHash& target_client_tag_hash) const = 0;
  virtual sync_pb::UniquePosition UniquePositionForInitialEntity(
      const ClientTagHash& target_client_tag_hash) const = 0;

  // Returns the current unique position for the tracked entity. Note that
  // default proto could be returned (for example if the entity did not have
  // unique position before).
  virtual sync_pb::UniquePosition GetUniquePositionForStorageKey(
      const std::string& storage_key) const = 0;

  virtual base::WeakPtr<DataTypeLocalChangeProcessor> GetWeakPtr() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_LOCAL_CHANGE_PROCESSOR_H_
