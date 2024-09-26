// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"
#include "components/sync_sessions/local_session_event_handler_impl.h"
#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"
#include "components/sync_sessions/session_store.h"
#include "components/sync_sessions/sessions_global_id_mapper.h"

namespace sync_sessions {

class LocalSessionEventRouter;
class SyncSessionsClient;

// Sync bridge implementation for SESSIONS data type. Takes care of propagating
// local sessions to other clients as well as providing a representation of
// foreign sessions.
//
// This is achieved by implementing the interface DataTypeSyncBridge, which
// ClientTagBasedDataTypeProcessor will use to interact, ultimately, with the
// sync server. See
// https://www.chromium.org/developers/design-documents/sync/model-api/#implementing-datatypesyncbridge
// for details.
class SessionSyncBridge : public syncer::DataTypeSyncBridge,
                          public LocalSessionEventHandlerImpl::Delegate {
 public:
  // Raw pointers must not be null and their pointees must outlive this object.
  SessionSyncBridge(
      const base::RepeatingClosure& notify_foreign_session_updated_cb,
      SyncSessionsClient* sessions_client,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  SessionSyncBridge(const SessionSyncBridge&) = delete;
  SessionSyncBridge& operator=(const SessionSyncBridge&) = delete;

  ~SessionSyncBridge() override;

  SessionsGlobalIdMapper* GetGlobalIdMapper();
  OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  bool IsLocalDataOutOfSyncForTest() const;

  // DataTypeSyncBridge implementation.
  void OnSyncStarting(
      const syncer::DataTypeActivationRequest& request) override;
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
  void OnSyncPaused() override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // LocalSessionEventHandlerImpl::Delegate implementation.
  std::unique_ptr<LocalSessionEventHandlerImpl::WriteBatch>
  CreateLocalSessionWriteBatch() override;
  bool IsTabNodeUnsynced(int tab_node_id) override;
  void TrackLocalNavigationId(base::Time timestamp, int unique_id) override;

 private:
  void OnStoreInitialized(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<SessionStore> store,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void StartLocalSessionEventHandler(bool is_new_session);
  void DeleteForeignSessionFromUI(const std::string& tag);
  void DoGarbageCollection(SessionStore::WriteBatch* write_batch);
  std::unique_ptr<SessionStore::WriteBatch> CreateSessionStoreWriteBatch();
  void DeleteForeignSessionWithBatch(const std::string& session_tag,
                                     SessionStore::WriteBatch* batch);
  void ResubmitLocalSession();
  void ReportError(const syncer::ModelError& error);

  const base::RepeatingClosure notify_foreign_session_updated_cb_;
  const raw_ptr<SyncSessionsClient> sessions_client_;
  const raw_ptr<LocalSessionEventRouter> local_session_event_router_;

  SessionsGlobalIdMapper global_id_mapper_;

  std::unique_ptr<SessionStore> store_;
  // If `store_` has previously been created, but was cleared during
  // `ApplyDisableSyncChanges()`, then this callback allows the bridge to
  // synchronously re-create an empty `SessionStore`.
  SessionStore::RecreateEmptyStoreCallback recreate_empty_store_callback_;

  // All data dependent on sync being starting or started.
  struct SyncingState {
    SyncingState();
    ~SyncingState();

    std::unique_ptr<OpenTabsUIDelegateImpl> open_tabs_ui_delegate;
    std::unique_ptr<LocalSessionEventHandlerImpl> local_session_event_handler;
  };

  // TODO(mastiz): We should rather rename this to |syncing_state_|.
  // Non-empty while sync is active, i.e. started and not paused.
  std::optional<SyncingState> syncing_;

  base::WeakPtrFactory<SessionSyncBridge> weak_ptr_factory_{this};
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_BRIDGE_H_
