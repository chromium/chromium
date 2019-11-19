// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_FAKE_SYNC_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_FAKE_SYNC_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/sync/engine/fake_model_type_connector.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/test/fake_sync_encryption_handler.h"

namespace base {
class SequencedTaskRunner;
}

namespace syncer {

class FakeSyncEncryptionHandler;

class FakeSyncManager : public SyncManager {
 public:
  // |initial_sync_ended_types|: The set of types that have initial_sync_ended
  // set to true. This value will be used by InitialSyncEndedTypes() until the
  // next configuration is performed.
  //
  // |progress_marker_types|: The set of types that have valid progress
  // markers. This will be used by GetTypesWithEmptyProgressMarkerToken() until
  // the next configuration is performed.
  //
  // |configure_fail_types|: The set of types that will fail
  // configuration. Once ConfigureSyncer is called, the
  // |initial_sync_ended_types_| and |progress_marker_types_| will be updated
  // to include those types that didn't fail.
  FakeSyncManager(ModelTypeSet initial_sync_ended_types,
                  ModelTypeSet progress_marker_types,
                  ModelTypeSet configure_fail_types,
                  bool should_fail_on_init);
  ~FakeSyncManager() override;

  // Returns those types that have been purged from the directory since the last
  // call to GetAndResetPurgedTypes(), or since startup if never called.
  ModelTypeSet GetAndResetPurgedTypes();

  // Returns those types that have been unapplied as part of purging disabled
  // types since the last call to GetAndResetUnappliedTypes, or since startup if
  // never called.
  ModelTypeSet GetAndResetUnappliedTypes();

  // Returns those types that have been downloaded since the last call to
  // GetAndResetDownloadedTypes(), or since startup if never called.
  ModelTypeSet GetAndResetDownloadedTypes();

  // Returns the types that have most recently received a refresh request.
  ModelTypeSet GetLastRefreshRequestTypes();

  // Returns the most recent configuration reason since the last call to
  // GetAndResetConfigureReason, or since startup if never called.
  ConfigureReason GetAndResetConfigureReason();

  // Returns the number of invalidations received since startup.
  int GetInvalidationCount() const;

  // Block until the sync thread has finished processing any pending messages.
  void WaitForSyncThread();

  // SyncManager implementation.
  // Note: we treat whatever message loop this is called from as the sync
  // loop for purposes of callbacks.
  void Init(InitArgs* args) override;
  ModelTypeSet InitialSyncEndedTypes() override;
  ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) override;
  void PurgePartiallySyncedTypes() override;
  void PurgeDisabledTypes(ModelTypeSet to_purge,
                          ModelTypeSet to_journal,
                          ModelTypeSet to_unapply) override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  void StartSyncingNormally(base::Time last_poll_time) override;
  void StartConfiguration() override;
  void ConfigureSyncer(ConfigureReason reason,
                       ModelTypeSet to_download,
                       SyncFeatureState sync_feature_state,
                       const base::Closure& ready_task) override;
  void OnIncomingInvalidation(
      ModelType type,
      std::unique_ptr<InvalidationInterface> interface) override;
  void SetInvalidatorEnabled(bool invalidator_enabled) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  SyncStatus GetDetailedStatus() const override;
  void SaveChanges() override;
  void ShutdownOnSyncThread() override;
  UserShare* GetUserShare() override;
  ModelTypeConnector* GetModelTypeConnector() override;
  std::unique_ptr<ModelTypeConnector> GetModelTypeConnectorProxy() override;
  std::string cache_guid() override;
  std::string birthday() override;
  std::string bag_of_chips() override;
  bool HasUnsyncedItemsForTest() override;
  SyncEncryptionHandler* GetEncryptionHandler() override;
  std::vector<std::unique_ptr<ProtocolEvent>> GetBufferedProtocolEvents()
      override;
  void RefreshTypes(ModelTypeSet types) override;
  void RegisterDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) override;
  void UnregisterDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) override;
  bool HasDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) override;
  void RequestEmitDebugInfo() override;
  void OnCookieJarChanged(bool account_mismatch, bool empty_jar) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  void UpdateInvalidationClientId(const std::string&) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  base::ObserverList<SyncManager::Observer>::Unchecked observers_;

  bool should_fail_on_init_;
  // Faked directory state.
  ModelTypeSet initial_sync_ended_types_;
  ModelTypeSet progress_marker_types_;

  // Test specific state.
  // The types that should fail configuration attempts. These types will not
  // have their progress markers or initial_sync_ended bits set.
  ModelTypeSet configure_fail_types_;
  // The set of types that have been purged.
  ModelTypeSet purged_types_;
  // Subset of |purged_types_| that were unapplied.
  ModelTypeSet unapplied_types_;
  // The set of types that have been downloaded.
  ModelTypeSet downloaded_types_;

  // The types for which a refresh was most recently requested.
  ModelTypeSet last_refresh_request_types_;

  // The most recent configure reason.
  ConfigureReason last_configure_reason_;

  FakeSyncEncryptionHandler fake_encryption_handler_;

  FakeModelTypeConnector fake_model_type_connector_;

  TestUserShare test_user_share_;

  // Number of invalidations received since startup.
  int num_invalidations_received_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncManager);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_FAKE_SYNC_MANAGER_H_
