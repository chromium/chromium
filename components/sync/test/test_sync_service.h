// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TEST_TEST_SYNC_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace syncer {

// A simple test implementation of SyncService that allows direct control over
// the returned state. By default, everything returns "enabled"/"active".
class TestSyncService : public SyncService {
 public:
  TestSyncService();

  TestSyncService(const TestSyncService&) = delete;
  TestSyncService& operator=(const TestSyncService&) = delete;

  ~TestSyncService() override;

  // High-level setters that configure common scenarios. Passing
  // ConsentLevel::kSync will also mark the first sync-the-feature setup as
  // complete.
  void SetSignedIn(signin::ConsentLevel consent_level);
  void SetSignedIn(signin::ConsentLevel consent_level,
                   const CoreAccountInfo& account_info);
  void SetSignedOut();

  // Mimics the user resetting Sync from the web dashboard. On ChromeOS Ash,
  // this also flips SetSyncFeatureDisabledViaDashboard().
  void MimicDashboardClear();

  // Controls DISABLE_REASON_ENTERPRISE_POLICY and consequently
  // GetTransportState(). The default is true.
  void SetAllowedByEnterprisePolicy(bool allowed);

  // Controls DISABLE_REASON_UNRECOVERABLE_ERROR and consequently
  // GetTransportState(). The default is false.
  void SetHasUnrecoverableError(bool has_error);

  // The "max transport state" is the one yielded by GetTransportState() *if*
  // there is no auth error set by SetPersistentAuthError() - in which the
  // TransportState would be PAUSED - and no DisableReason - in which case it'd
  // be DISABLED. The default "max transport state" is ACTIVE.
  // Use this method only to test intermediate states like INITIALIZING or
  // START_DEFERRED. Calling with DISABLED or PAUSED will crash.
  void SetMaxTransportState(TransportState max_transport_state);

  void SetLocalSyncEnabled(bool local_sync_enabled);

  // Setters to mimic common auth error scenarios. Note that these functions
  // may change the transport state, as returned by GetTransportState().
  // SetSignedOut() resets the auth error.
  void SetPersistentAuthError();
  void ClearAuthError();

  void SetInitialSyncFeatureSetupComplete(
      bool initial_sync_feature_setup_complete);
  void SetFailedDataTypes(const DataTypeSet& types);

  void SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot);
  // Convenience versions of the above, for when the caller doesn't care about
  // the particular values in the snapshot, just whether there is one.
  void SetEmptyLastCycleSnapshot();
  void SetNonEmptyLastCycleSnapshot();
  void SetDetailedSyncStatus(bool engine_available, SyncStatus status);
  void SetPassphraseRequired();
  void SetTrustedVaultKeyRequired(bool required);
  void SetTrustedVaultRecoverabilityDegraded(bool degraded);
  void SetIsUsingExplicitPassphrase(bool enabled);
  void SetDownloadStatusFor(const DataTypeSet& types,
                            DataTypeDownloadStatus download_status);
  void SetTypesWithUnsyncedData(const DataTypeSet& types);
  void SetLocalDataDescriptions(
      const std::map<DataType, LocalDataDescription>& local_data_descriptions);

  // If the passed callback is non-null, every
  // SendExplicitPassphraseToPlatformClient() call will invoke it.
  // Otherwise, the method no-ops.
  void SetPassphrasePlatformClientCallback(
      const base::RepeatingClosure& send_passphrase_to_platform_client_cb);

  // The passed callback (if non-null) will be called on TriggerRefresh().
  void SetTriggerRefreshCallback(
      const base::RepeatingCallback<void(DataTypeSet)>& trigger_refresh_cb);

  void FireStateChanged();
  void FireSyncCycleCompleted();

  // Similar to `GetSetupInProgressHandle()` but doesn't require the caller to
  // handle the lifetime of `SyncSetupInProgressHandle`. It also means that it
  // cannot be undone.
  void SetSetupInProgress();

  // SyncService implementation.
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
#endif  // BUILDFLAG(IS_ANDROID)

  void SetSyncFeatureRequested() override;
  TestSyncUserSettings* GetUserSettings() override;
  const TestSyncUserSettings* GetUserSettings() const override;
  DisableReasonSet GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  UserActionableError GetUserActionableError() const override;
  bool IsLocalSyncEnabled() const override;
  CoreAccountInfo GetAccountInfo() const override;
  bool HasSyncConsent() const override;
  GoogleServiceAuthError GetAuthError() const override;
  base::Time GetAuthErrorTime() const override;
  bool RequiresClientUpgrade() const override;

  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;

  DataTypeSet GetPreferredDataTypes() const override;
  DataTypeSet GetActiveDataTypes() const override;
  DataTypeSet GetTypesWithPendingDownloadForInitialSync() const override;
  void OnDataTypeRequestsSyncStartup(DataType type) override;
  void TriggerRefresh(const DataTypeSet& types) override;
  void DataTypePreconditionChanged(DataType type) override;

  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;

  SyncTokenStatus GetSyncTokenStatusForDebugging() const override;
  bool QueryDetailedSyncStatusForDebugging(SyncStatus* result) const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const override;
  TypeStatusMapForDebugging GetTypeStatusMapForDebugging() const override;
  void GetEntityCountsForDebugging(
      base::RepeatingCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  const GURL& GetSyncServiceUrlForDebugging() const override;
  std::string GetUnrecoverableErrorMessageForDebugging() const override;
  base::Location GetUnrecoverableErrorLocationForDebugging() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void GetAllNodesForDebugging(
      base::OnceCallback<void(base::Value::List)> callback) override;
  DataTypeDownloadStatus GetDownloadStatusFor(DataType type) const override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void SendExplicitPassphraseToPlatformClient() override;
  void GetTypesWithUnsyncedData(
      DataTypeSet requested_types,
      base::OnceCallback<void(DataTypeSet)> cb) const override;
  void GetLocalDataDescriptions(
      DataTypeSet types,
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>
          callback) override;
  void TriggerLocalDataMigration(DataTypeSet types) override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  void OnSetupInProgressHandleDestroyed();

  TestSyncUserSettings user_settings_;
  DisableReasonSet disable_reasons_;
  TransportState max_transport_state_ = TransportState::ACTIVE;
  bool has_persistent_auth_error_ = false;
  bool local_sync_enabled_ = false;
  CoreAccountInfo account_info_;
  bool has_sync_consent_ = true;
  int outstanding_setup_in_progress_handles_ = 0;

  DataTypeSet failed_data_types_;

  std::map<DataType, DataTypeDownloadStatus> download_statuses_;

  bool detailed_sync_status_engine_available_ = false;
  SyncStatus detailed_sync_status_;

  SyncCycleSnapshot last_cycle_snapshot_;

  base::ObserverList<SyncServiceObserver>::UncheckedAndDanglingUntriaged
      observers_;

  GURL sync_service_url_;

  DataTypeSet unsynced_types_;

  std::map<DataType, LocalDataDescription> local_data_descriptions_;

  // Nullable.
  base::RepeatingClosure send_passphrase_to_platform_client_cb_;

  // Nullable.
  base::RepeatingCallback<void(syncer::DataTypeSet)> trigger_refresh_cb_;

  base::WeakPtrFactory<TestSyncService> weak_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_SYNC_SERVICE_H_
