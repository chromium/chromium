// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_STORE_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_STORE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/synced_session_tracker.h"

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
    syncer::DeviceInfo::FormFactor device_form_factor =
        syncer::DeviceInfo::FormFactor::kUnknown;
  };

  using OpenCallback = base::OnceCallback<void(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<SessionStore> store,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch)>;

  // Opens a SessionStore instance, which involves IO to load previous state
  // from disk. |sessions_client| must not be null and must outlive the
  // SessionStore instance returned via |callback|, or until the callback is
  // cancelled.
  static void Open(const std::string& cache_guid,
                   SyncSessionsClient* sessions_client,
                   OpenCallback callback);

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

  // Similar to DataTypeStore::WriteBatch but enforces a consistent state. In
  // the current implementation, some functions do *NOT* update the tracker, so
  // callers are responsible for doing so.
  // TODO(crbug.com/41295474): Enforce consistency between in-memory and
  // persisted data by always updating the tracker.
  class WriteBatch {
   public:
    // Callback that mimics the signature of DataTypeStore::CommitWriteBatch().
    using CommitCallback = base::OnceCallback<void(
        std::unique_ptr<syncer::DataTypeStore::WriteBatch>,
        syncer::DataTypeStore::CallbackWithResult)>;

    // Raw pointers must not be nullptr and must outlive this object.
    WriteBatch(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch,
               CommitCallback commit_cb,
               syncer::OnceModelErrorHandler error_handler,
               SyncedSessionTracker* session_tracker);

    WriteBatch(const WriteBatch&) = delete;
    WriteBatch& operator=(const WriteBatch&) = delete;

    ~WriteBatch();

    // Most mutations below return storage keys.
    std::string PutAndUpdateTracker(const sync_pb::SessionSpecifics& specifics,
                                    base::Time modification_time);
    // Returns all deleted storage keys, which may be more than one if
    // |storage_key| refers to a header entity.
    std::vector<std::string> DeleteForeignEntityAndUpdateTracker(
        const std::string& storage_key);
    // The functions below do not update SyncedSessionTracker and hence it is
    // the caller's responsibility to do so *before* calling these functions.
    std::string PutWithoutUpdatingTracker(
        const sync_pb::SessionSpecifics& specifics);
    std::string DeleteLocalTabWithoutUpdatingTracker(int tab_node_id);

    syncer::MetadataChangeList* GetMetadataChangeList();

    static void Commit(std::unique_ptr<WriteBatch> batch);

   private:
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch_;
    CommitCallback commit_cb_;
    syncer::OnceModelErrorHandler error_handler_;
    const raw_ptr<SyncedSessionTracker> session_tracker_;
  };

  SessionStore(const SessionStore&) = delete;
  SessionStore& operator=(const SessionStore&) = delete;

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

  using RecreateEmptyStoreCallback =
      base::OnceCallback<std::unique_ptr<SessionStore>(
          const std::string& cache_guid,
          SyncSessionsClient* sessions_client)>;

  // Deletes all data and metadata from the `session_store` and destroys it.
  // Returns a callback that allows synchronously re-creating an empty
  // SessionStore, by reusing the underlying DataTypeStore.
  static RecreateEmptyStoreCallback DeleteAllDataAndMetadata(
      std::unique_ptr<SessionStore> session_store);

  // TODO(crbug.com/41295474): Avoid exposing a mutable tracker, because that
  // bypasses the consistency-enforcing API.
  SyncedSessionTracker* mutable_tracker() { return &session_tracker_; }
  const SyncedSessionTracker* tracker() const { return &session_tracker_; }

 private:
  // Helper class used to collect all parameters needed by the constructor.
  struct Builder;

  static void OnStoreCreated(
      std::unique_ptr<Builder> builder,
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore> underlying_store);
  static void OnReadAllMetadata(
      std::unique_ptr<Builder> builder,
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  static void OnReadAllData(std::unique_ptr<Builder> builder,
                            const std::optional<syncer::ModelError>& error);

  static std::unique_ptr<SessionStore> RecreateEmptyStore(
      SessionInfo local_session_info_without_session_tag,
      std::unique_ptr<syncer::DataTypeStore> underlying_store,
      const std::string& cache_guid,
      SyncSessionsClient* sessions_client);

  // |sessions_client| must not be null and must outlive this object.
  SessionStore(const SessionInfo& local_session_info,
               std::unique_ptr<syncer::DataTypeStore> underlying_store,
               std::map<std::string, sync_pb::SessionSpecifics> initial_data,
               const syncer::EntityMetadataMap& initial_metadata,
               SyncSessionsClient* sessions_client);

  const SessionInfo local_session_info_;

  // In charge of actually persisting changes to disk.
  std::unique_ptr<syncer::DataTypeStore> store_;

  const raw_ptr<SyncSessionsClient> sessions_client_;

  SyncedSessionTracker session_tracker_;

  base::WeakPtrFactory<SessionStore> weak_ptr_factory_{this};
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_STORE_H_
