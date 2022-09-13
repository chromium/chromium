// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TEST_MOCK_SYNC_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "testing/gmock/include/gmock/gmock.h"

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
  MOCK_METHOD(DisableReasonSet, GetDisableReasons, (), (const override));
  MOCK_METHOD(TransportState, GetTransportState, (), (const override));
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
  MOCK_METHOD(ModelTypeSet, GetPreferredDataTypes, (), (const override));
  MOCK_METHOD(ModelTypeSet, GetActiveDataTypes, (), (const override));
  MOCK_METHOD(void, StopAndClear, (), (override));
  MOCK_METHOD(void,
              OnDataTypeRequestsSyncStartup,
              (ModelType type),
              (override));
  MOCK_METHOD(void, TriggerRefresh, (const ModelTypeSet& types), (override));
  MOCK_METHOD(void,
              DataTypePreconditionChanged,
              (syncer::ModelType type),
              (override));
  MOCK_METHOD(void,
              SetInvalidationsForSessionsEnabled,
              (bool enabled),
              (override));
  MOCK_METHOD(void,
              AddTrustedVaultDecryptionKeysFromWeb,
              (const std::string& gaia_id,
               const std::vector<std::vector<uint8_t>>& keys,
               int last_key_version),
              (override));
  MOCK_METHOD(void,
              AddTrustedVaultRecoveryMethodFromWeb,
              (const std::string& gaia_id,
               const std::vector<uint8_t>& public_key,
               int method_type_hint,
               base::OnceClosure callback),
              (override));
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
  MOCK_METHOD(base::Value::List,
              GetTypeStatusMapForDebugging,
              (),
              (const override));
  MOCK_METHOD(void,
              GetEntityCountsForDebugging,
              (base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)>),
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

  // KeyedService implementation.
  MOCK_METHOD(void, Shutdown, (), (override));

 private:
  testing::NiceMock<SyncUserSettingsMock> user_settings_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_SYNC_SERVICE_H_
