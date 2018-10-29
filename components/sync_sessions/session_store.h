// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_STORE_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync_sessions/synced_session_tracker.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

namespace sync_sessions {

// Class responsible for maintaining an in-memory representation of sync
// sessions (by owning a SyncedSessionTracker) with the capability to persist
// state to disk and restore (data and metadata). The API enforces a valid and
// consistent state of the model, e.g. by making sure there is at most one sync
// entity per client tag.
class SessionStore {
 public:
  struct SessionInfo {
    std::string session_tag;
    std::string client_name;
    sync_pb::SyncEnums::DeviceType device_type = sync_pb::SyncEnums::TYPE_UNSET;
  };

  // Creation factory. The instantiation process is quite complex because it
  // loads state from disk.
  using FactoryCompletionCallback = base::OnceCallback<void(
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<SessionStore> store,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch)>;
  using Factory =
      base::RepeatingCallback<void(const syncer::DeviceInfo& device_info,
                                   FactoryCompletionCallback callback)>;
  // Mimics signature of FaviconCache::UpdateMappingsFromForeignTab().
  using RestoredForeignTabCallback =
      base::RepeatingCallback<void(const sync_pb::SessionTab&, base::Time)>;

  // Creates a factory object that is capable of constructing instances of type
  // |SessionStore| and handling the involved IO. |sessions_client| must not be
  // null and must outlive the factory as well as the instantiated stores.
  static Factory CreateFactory(
      SyncSessionsClient* sessions_client,
      const RestoredForeignTabCallback& restored_foreign_tab_callback);

  // Verifies whether a proto is malformed (e.g. required fields are missing).
  static bool AreValidSpecifics(const sync_pb::SessionSpecifics& specifics);
  // |specifics| must be valid, see AreValidSpecifics().
  static std::string GetClientTag(const sync_pb::SessionSpecifics& specifics);
  // |specifics| must be valid, see AreValidSpecifics().
  static std::string GetStorageKey(const sync_pb::SessionSpecifics& specifics);
  static std::string GetHeaderStorageKey(const std::string& session_tag);
  static std::string GetTabStorageKey(const std::string& session_tag,
                                      int tab_node_id);
  // Verifies if |storage_key| corresponds to an entity in the local session,
  // identified by the session tag.
  bool StorageKeyMatchesLocalSession(const std::string& storage_key) const;

  // Various equivalents for testing.
  static std::string GetTabClientTagForTest(const std::string& session_tag,
                                            int tab_node_id);

  // Similar to ModelTypeStore::WriteBatch but enforces a consistent state. In
  // the current implementation, some functions do *NOT* update the tracker, so
  // callers are responsible for doing so.
  // TODO(crbug.com/681921): Enforce consistency between in-memory and persisted
  // data by always updating the tracker.
  class WriteBatch {
   public:
    // Callback that mimics the signature of ModelTypeStore::CommitWriteBatch().
    using CommitCallback = base::OnceCallback<void(
        std::unique_ptr<syncer::ModelTypeStore::WriteBatch>,
        syncer::ModelTypeStore::CallbackWithResult)>;

    // Raw pointers must not be nullptr and must outlive this object.
    WriteBatch(std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch,
               CommitCallback commit_cb,
               syncer::OnceModelErrorHandler error_handler,
               SyncedSessionTracker* session_tracker);
    ~WriteBatch();

    // Most mutations below return a storage key.
    std::string PutAndUpdateTracker(const sync_pb::SessionSpecifics& specifics,
                                    base::Time modification_time);
    void DeleteForeignEntityAndUpdateTracker(const std::string& storage_key);
    // The functions below do not update SyncedSessionTracker and hence it is
    // the caller's responsibility to do so *before* calling these functions.
    std::string PutWithoutUpdatingTracker(
        const sync_pb::SessionSpecifics& specifics);
    std::string DeleteLocalTabWithoutUpdatingTracker(int tab_node_id);

    syncer::MetadataChangeList* GetMetadataChangeList();

    static void Commit(std::unique_ptr<WriteBatch> batch);

   private:
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch_;
    CommitCallback commit_cb_;
    syncer::OnceModelErrorHandler error_handler_;
    SyncedSessionTracker* const session_tracker_;

    DISALLOW_COPY_AND_ASSIGN(WriteBatch);
  };

  // Construction once all data and metadata has been loaded from disk. Use
  // the factory above to take care of the IO. |sessions_client| must not be
  // null and must outlive this object.
  SessionStore(SyncSessionsClient* sessions_client,
               const SessionInfo& local_session_info,
               std::unique_ptr<syncer::ModelTypeStore> store,
               std::map<std::string, sync_pb::SessionSpecifics> initial_data,
               const syncer::EntityMetadataMap& initial_metadata,
               const RestoredForeignTabCallback& restored_foreign_tab_callback);
  ~SessionStore();

  const SessionInfo& local_session_info() const { return local_session_info_; }

  // Converts the in-memory model (SyncedSessionTracker) of sessions to sync
  // protos.
  std::unique_ptr<syncer::DataBatch> GetSessionDataForKeys(
      const std::vector<std::string>& storage_keys) const;

  // Returns all known session entities, local and foreign, generated from the
  // in-memory model (SyncedSessionTracker).
  std::unique_ptr<syncer::DataBatch> GetAllSessionData() const;

  // Write API. WriteBatch instances must not outlive this store and must be
  // committed prior to destruction. Besides, more than one uncommitted
  // instance must not exist at any time.
  std::unique_ptr<WriteBatch> CreateWriteBatch(
      syncer::OnceModelErrorHandler error_handler);
  void DeleteAllDataAndMetadata();

  // TODO(crbug.com/681921): Avoid exposing a mutable tracker, because that
  // bypasses the consistency-enforcing API.
  SyncedSessionTracker* mutable_tracker() { return &session_tracker_; }
  const SyncedSessionTracker* tracker() const { return &session_tracker_; }

 private:
  // In charge of actually persisting changes to disk.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  const SessionInfo local_session_info_;

  SyncedSessionTracker session_tracker_;

  base::WeakPtrFactory<SessionStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SessionStore);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_STORE_H_
