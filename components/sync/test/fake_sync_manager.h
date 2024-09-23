// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_MANAGER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/test/fake_data_type_connector.h"
#include "components/sync/test/fake_sync_encryption_handler.h"

namespace base {
class SequencedTaskRunner;
}

namespace syncer {

class FakeSyncEncryptionHandler;
class SyncCycleSnapshot;

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
  FakeSyncManager(DataTypeSet initial_sync_ended_types,
                  DataTypeSet progress_marker_types,
                  DataTypeSet configure_fail_types);

  FakeSyncManager(const FakeSyncManager&) = delete;
  FakeSyncManager& operator=(const FakeSyncManager&) = delete;

  ~FakeSyncManager() override;

  // Returns those types that have been downloaded since the last call to
  // GetAndResetDownloadedTypes(), or since startup if never called.
  DataTypeSet GetAndResetDownloadedTypes();

  // Returns the types that have most recently received a refresh request.
  DataTypeSet GetLastRefreshRequestTypes();

  // Returns the most recent configuration reason since the last call to
  // GetAndResetConfigureReason, or since startup if never called.
  ConfigureReason GetAndResetConfigureReason();

  // Returns the number of invalidations received for type since startup.
  int GetInvalidationCount(DataType type) const;

  // Block until the sync thread has finished processing any pending messages.
  void WaitForSyncThread();

  bool IsInvalidatorEnabled() const { return invalidator_enabled_; }

  // Notifies all observers about the changed |status|.
  void NotifySyncStatusChanged(const SyncStatus& status);

  // Notifies |observers_| about sync cycle completion.
  void NotifySyncCycleCompleted(const SyncCycleSnapshot& snapshot);

  // SyncManager implementation.
  // Note: we treat whatever message loop this is called from as the sync
  // loop for purposes of callbacks.
  void Init(InitArgs* args) override;
  DataTypeSet InitialSyncEndedTypes() override;
  DataTypeSet GetConnectedTypes() override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  void StartSyncingNormally(base::Time last_poll_time) override;
  void StartConfiguration() override;
  void ConfigureSyncer(ConfigureReason reason,
                       DataTypeSet to_download,
                       SyncFeatureState sync_feature_state,
                       base::OnceClosure ready_task) override;
  void OnIncomingInvalidation(
      DataType type,
      std::unique_ptr<SyncInvalidation> interface) override;
  void SetInvalidatorEnabled(bool invalidator_enabled) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void ShutdownOnSyncThread() override;
  DataTypeConnector* GetDataTypeConnector() override;
  std::unique_ptr<DataTypeConnector> GetDataTypeConnectorProxy() override;
  std::string cache_guid() override;
  std::string birthday() override;
  std::string bag_of_chips() override;
  bool HasUnsyncedItemsForTest() override;
  SyncEncryptionHandler* GetEncryptionHandler() override;
  std::vector<std::unique_ptr<ProtocolEvent>> GetBufferedProtocolEvents()
      override;
  void RefreshTypes(DataTypeSet types) override;
  void OnCookieJarChanged(bool account_mismatch) override;
  void UpdateActiveDevicesInvalidationInfo(
      ActiveDevicesInvalidationInfo active_devices_invalidation_info) override;

 private:
  void DoNotifySyncStatusChanged(const SyncStatus& status);
  void DoNotifySyncCycleCompleted(const SyncCycleSnapshot& snapshot);

  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  base::ObserverList<SyncManager::Observer>::Unchecked observers_;

  std::string cache_guid_;
  std::string birthday_;
  std::string bag_of_chips_;
  bool invalidator_enabled_ = false;

  // Faked data state.
  DataTypeSet initial_sync_ended_types_;
  DataTypeSet progress_marker_types_;

  // Test specific state.
  // The types that should fail configuration attempts. These types will not
  // have their progress markers or initial_sync_ended bits set.
  DataTypeSet configure_fail_types_;

  // The set of types that have been downloaded.
  DataTypeSet downloaded_types_;

  // The types for which a refresh was most recently requested.
  DataTypeSet last_refresh_request_types_;

  // The most recent configure reason.
  ConfigureReason last_configure_reason_;

  FakeSyncEncryptionHandler fake_encryption_handler_;

  FakeDataTypeConnector fake_data_type_connector_;

  // Number of invalidations received per type since startup.
  std::map<DataType, int> num_invalidations_received_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_MANAGER_H_
