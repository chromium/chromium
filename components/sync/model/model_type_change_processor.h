// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

class ClientTagHash;
class MetadataBatch;
class MetadataChangeList;
class ModelTypeSyncBridge;

// Interface used by the ModelTypeSyncBridge to inform sync of local changes.
class ModelTypeChangeProcessor {
 public:
  ModelTypeChangeProcessor();
  virtual ~ModelTypeChangeProcessor();

  // Inform the processor of a new or updated entity. The |entity_data| param
  // does not need to be fully set, but it should at least have specifics and
  // non-unique name. The processor will fill in the rest if the bridge does
  // not have a reason to care. For example, if |client_tag_hash| is not set,
  // the bridge's GetClientTag() will be exercised (and must be supported).
  virtual void Put(const std::string& storage_key,
                   std::unique_ptr<EntityData> entity_data,
                   MetadataChangeList* metadata_change_list) = 0;

  // Inform the processor of a deleted entity. The call is ignored if
  // |storage_key| is unknown.
  virtual void Delete(const std::string& storage_key,
                      MetadataChangeList* metadata_change_list) = 0;

  // Sets storage key for the new entity. This function only applies to
  // datatypes that can't generate storage key based on EntityData. Bridge
  // should call this function when handling MergeSyncData/ApplySyncChanges to
  // inform the processor about |storage_key| of an entity identified by
  // |entity_data|. Metadata changes about new entity will be appended to
  // |metadata_change_list|.
  virtual void UpdateStorageKey(const EntityData& entity_data,
                                const std::string& storage_key,
                                MetadataChangeList* metadata_change_list) = 0;

  // Remove entity metadata and do not track the entity. Applies to datatypes
  // that support local deletions that should not get synced up (e.g. TYPED_URL
  // does not sync up deletions of expired URLs). If the deletion should get
  // synced up, use change_processor()->Delete() instead. The call is ignored if
  // |storage_key| is unknown.
  virtual void UntrackEntityForStorageKey(const std::string& storage_key) = 0;

  // Remove entity metadata and do not track the entity, exactly like
  // UntrackEntityForStorageKey() above. This function should only be called by
  // datatypes that can't generate storage keys. The call is ignored if
  // |client_tag_hash| is unknown.
  virtual void UntrackEntityForClientTagHash(
      const ClientTagHash& client_tag_hash) = 0;

  // Returns true if a tracked entity has local changes. A commit may or may not
  // be in progress at this time.
  // TODO(mastiz): The only user of this is HISTORY_DELETE_DIRECTIVES which
  // needs it for a rather questionable reason. Revisit this, for example by
  // moving the SyncableService to history's backend thread, and leveraging
  // USS's ability to delete local data upcon commit completion.
  virtual bool IsEntityUnsynced(const std::string& storage_key) = 0;

  // Returns the creation timestamp of the sync entity, or a null time if the
  // entity is not tracked.
  virtual base::Time GetEntityCreationTime(
      const std::string& storage_key) const = 0;

  // Returns the modification timestamp of the sync entity, or a null time if
  // the entity is not tracked.
  virtual base::Time GetEntityModificationTime(
      const std::string& storage_key) const = 0;

  // Pass the pointer to the processor so that the processor can notify the
  // bridge of various events; |bridge| must not be nullptr and must outlive
  // this object.
  virtual void OnModelStarting(ModelTypeSyncBridge* bridge) = 0;

  // The |bridge| is expected to call this exactly once unless it encounters an
  // error. Ideally ModelReadyToSync() is called as soon as possible during
  // initialization, and must be called before invoking either Put() or
  // Delete(). The bridge needs to be able to synchronously handle
  // MergeSyncData() and ApplySyncChanges() after calling ModelReadyToSync(). If
  // an error is encountered, calling ReportError() instead is sufficient.
  virtual void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) = 0;

  // Returns a boolean representing whether the processor's metadata is
  // currently up to date and accurately tracking the model type's data. If
  // false, and ModelReadyToSync() has already been called, then Put and Delete
  // will no-op and can be omitted by bridge.
  virtual bool IsTrackingMetadata() = 0;

  // Returns the account ID for which metadata is being tracked, or empty if not
  // tracking metadata.
  virtual std::string TrackedAccountId() = 0;

  // Returns the cache guid for which metadata is being tracked, or empty if not
  // tracking metadata.
  virtual std::string TrackedCacheGuid() = 0;

  // Report an error in the model to sync. Should be called for any persistence
  // or consistency error the bridge encounters outside of a method that allows
  // returning a ModelError directly. Outstanding callbacks are not expected to
  // be called after an error. This will result in sync being temporarily
  // disabled for the model type (generally until the next restart).
  virtual void ReportError(const ModelError& error) = 0;

  // Returns whether the processor has encountered any error, either reported
  // by the bridge via ReportError() or by other means.
  virtual base::Optional<ModelError> GetError() const = 0;

  // Returns the delegate for the controller.
  virtual base::WeakPtr<ModelTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_
