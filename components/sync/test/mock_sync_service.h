// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TEST_MOCK_SYNC_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace syncer {

// Mock implementation of SyncService. You probably don't need this; look at
// TestSyncService instead!
// As one special case compared to a regular mock, the GetUserSettings() methods
// are not mocked. Instead, they return a mock version of SyncUserSettings.
class MockSyncService : public SyncService {
 public:
  MockSyncService();
  ~MockSyncService() override;

  syncer::SyncUserSettingsMock* GetMockUserSettings();

  // SyncService implementation.
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              GetJavaObject,
              (),
              (override));
#endif  // BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, SetSyncFeatureRequested, (), (override));
  MOCK_METHOD(DisableReasonSet, GetDisableReasons, (), (const override));
  MOCK_METHOD(TransportState, GetTransportState, (), (const override));
  MOCK_METHOD(UserActionableError,
              GetUserActionableError,
              (),
              (const override));
  MOCK_METHOD(bool, IsLocalSyncEnabled, (), (const override));
  MOCK_METHOD(CoreAccountInfo, GetAccountInfo, (), (const override));
  MOCK_METHOD(bool, HasSyncConsent, (), (const override));
  MOCK_METHOD(GoogleServiceAuthError, GetAuthError, (), (const override));
  MOCK_METHOD(base::Time, GetAuthErrorTime, (), (const override));
  MOCK_METHOD(bool, RequiresClientUpgrade, (), (const override));
  MOCK_METHOD(std::unique_ptr<SyncSetupInProgressHandle>,
              GetSetupInProgressHandle,
              (),
              (override));
  MOCK_METHOD(bool, IsSetupInProgress, (), (const override));
  MOCK_METHOD(DataTypeSet, GetPreferredDataTypes, (), (const override));
  MOCK_METHOD(DataTypeSet, GetActiveDataTypes, (), (const override));
  MOCK_METHOD(DataTypeSet,
              GetTypesWithPendingDownloadForInitialSync,
              (),
              (const override));
  MOCK_METHOD(void, OnDataTypeRequestsSyncStartup, (DataType type), (override));
  MOCK_METHOD(void, TriggerRefresh, (const DataTypeSet& types), (override));
  MOCK_METHOD(void,
              DataTypePreconditionChanged,
              (syncer::DataType type),
              (override));
  MOCK_METHOD(void,
              SetInvalidationsForSessionsEnabled,
              (bool enabled),
              (override));
  MOCK_METHOD(void, SendExplicitPassphraseToPlatformClient, (), (override));
  MOCK_METHOD(void, AddObserver, (SyncServiceObserver * observer), (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (SyncServiceObserver * observer),
              (override));
  MOCK_METHOD(bool,
              HasObserver,
              (const SyncServiceObserver* observer),
              (const override));
  MOCK_METHOD(SyncTokenStatus,
              GetSyncTokenStatusForDebugging,
              (),
              (const override));
  MOCK_METHOD(bool,
              QueryDetailedSyncStatusForDebugging,
              (SyncStatus * result),
              (const override));
  MOCK_METHOD(base::Time, GetLastSyncedTimeForDebugging, (), (const override));
  MOCK_METHOD(SyncCycleSnapshot,
              GetLastCycleSnapshotForDebugging,
              (),
              (const override));
  MOCK_METHOD(TypeStatusMapForDebugging,
              GetTypeStatusMapForDebugging,
              (),
              (const override));
  MOCK_METHOD(void,
              GetEntityCountsForDebugging,
              (base::RepeatingCallback<void(const TypeEntitiesCount&)>),
              (const override));
  MOCK_METHOD(const GURL&, GetSyncServiceUrlForDebugging, (), (const override));
  MOCK_METHOD(std::string,
              GetUnrecoverableErrorMessageForDebugging,
              (),
              (const override));
  MOCK_METHOD(base::Location,
              GetUnrecoverableErrorLocationForDebugging,
              (),
              (const override));
  MOCK_METHOD(void,
              AddProtocolEventObserver,
              (ProtocolEventObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveProtocolEventObserver,
              (ProtocolEventObserver * observer),
              (override));
  MOCK_METHOD(void,
              GetAllNodesForDebugging,
              (base::OnceCallback<void(base::Value::List)> callback),
              (override));
  MOCK_METHOD(DataTypeDownloadStatus,
              GetDownloadStatusFor,
              (DataType type),
              (const override));
  MOCK_METHOD(void,
              GetTypesWithUnsyncedData,
              (DataTypeSet, base::OnceCallback<void(DataTypeSet)>),
              (const override));
  MOCK_METHOD(
      void,
      GetLocalDataDescriptions,
      (DataTypeSet types,
       base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>
           callback),
      (override));
  MOCK_METHOD(void, TriggerLocalDataMigration, (DataTypeSet types), (override));

  // KeyedService implementation.
  MOCK_METHOD(void, Shutdown, (), (override));

 private:
  testing::NiceMock<SyncUserSettingsMock> user_settings_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_SYNC_SERVICE_H_
