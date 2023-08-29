// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace syncer {

// Minimal fake implementation of SyncService. All methods return inactive/
// empty/null etc. Tests can subclass this to override the parts they need, but
// should consider using TestSyncService instead.
class FakeSyncService : public SyncService {
 public:
  FakeSyncService();
  ~FakeSyncService() override;

  // Dummy methods.
  // SyncService implementation.
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
#endif  // BUILDFLAG(IS_ANDROID)
  void SetSyncFeatureRequested() override;
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
  DisableReasonSet GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  UserActionableError GetUserActionableError() const override;
  CoreAccountInfo GetAccountInfo() const override;
  bool HasSyncConsent() const override;
  bool IsLocalSyncEnabled() const override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  ModelTypeSet GetActiveDataTypes() const override;
  ModelTypeSet GetTypesWithPendingDownloadForInitialSync() const override;
  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;
  void OnDataTypeRequestsSyncStartup(ModelType type) override;
  void StopAndClear() override;
  ModelTypeSet GetPreferredDataTypes() const override;
  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;
  GoogleServiceAuthError GetAuthError() const override;
  base::Time GetAuthErrorTime() const override;
  bool RequiresClientUpgrade() const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsSyncFeatureDisabledViaDashboard() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void DataTypePreconditionChanged(syncer::ModelType type) override;
  SyncTokenStatus GetSyncTokenStatusForDebugging() const override;
  bool QueryDetailedSyncStatusForDebugging(SyncStatus* result) const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const override;
  base::Value::List GetTypeStatusMapForDebugging() const override;
  void GetEntityCountsForDebugging(
      base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)> callback)
      const override;
  const GURL& GetSyncServiceUrlForDebugging() const override;
  std::string GetUnrecoverableErrorMessageForDebugging() const override;
  base::Location GetUnrecoverableErrorLocationForDebugging() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void GetAllNodesForDebugging(
      base::OnceCallback<void(base::Value::List)> callback) override;
  ModelTypeDownloadStatus GetDownloadStatusFor(ModelType type) const override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void GetTypesWithUnsyncedData(
      base::OnceCallback<void(ModelTypeSet)> cb) const override;
  void GetLocalDataDescriptions(
      ModelTypeSet types,
      base::OnceCallback<void(std::map<ModelType, LocalDataDescription>)>
          callback) override;
  void TriggerLocalDataMigration(ModelTypeSet types) override;

  // KeyedService implementation.
  void Shutdown() override;

 protected:
  bool IsSyncFeatureConsideredRequested() const override;

 private:
  GURL sync_service_url_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_SERVICE_H_
