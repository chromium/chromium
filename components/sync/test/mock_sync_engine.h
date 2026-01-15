// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_SYNC_ENGINE_H_
#define COMPONENTS_SYNC_TEST_MOCK_SYNC_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

// A mock of the SyncEngine.
//
// Note: If you don't really care about all the exact details, FakeSyncEngine is
// probably better.
class MockSyncEngine : public SyncEngine {
 public:
  MockSyncEngine();
  ~MockSyncEngine() override;

  // DataTypeConfigurer:
  MOCK_METHOD(void, ConfigureDataTypes, (ConfigureParams), (override));
  MOCK_METHOD(void,
              ConnectDataType,
              (DataType, std::unique_ptr<DataTypeActivationResponse>),
              (override));
  MOCK_METHOD(void, DisconnectDataType, (DataType), (override));

  // SyncEngine:
  MOCK_METHOD(void, Initialize, (InitParams), (override));
  MOCK_METHOD(bool, IsInitialized, (), (const override));
  MOCK_METHOD(void, TriggerRefresh, (const DataTypeSet&), (override));
  MOCK_METHOD(void, UpdateCredentials, (const SyncCredentials&), (override));
  MOCK_METHOD(void, InvalidateCredentials, (), (override));
  MOCK_METHOD(std::string, GetCacheGuid, (), (const override));
  MOCK_METHOD(std::string, GetBirthday, (), (const override));
  MOCK_METHOD(base::Time, GetLastSyncedTimeForDebugging, (), (const override));
  MOCK_METHOD(void, StartConfiguration, (), (override));
  MOCK_METHOD(void, StartSyncingWithServer, (), (override));
  MOCK_METHOD(void, StartHandlingInvalidations, (), (override));
  MOCK_METHOD(void,
              SetEncryptionPassphrase,
              (const std::string&, const KeyDerivationParams&),
              (override));
  MOCK_METHOD(void,
              SetExplicitPassphraseDecryptionKey,
              (std::unique_ptr<Nigori>),
              (override));
  MOCK_METHOD(void,
              AddTrustedVaultDecryptionKeys,
              (const std::vector<std::vector<uint8_t>>&, base::OnceClosure),
              (override));
  MOCK_METHOD(void, StopSyncingForShutdown, (), (override));
  MOCK_METHOD(void, Shutdown, (ShutdownReason), (override));
  MOCK_METHOD(const SyncStatus&, GetDetailedStatus, (), (const override));
  MOCK_METHOD(void,
              GetTypesWithUnsyncedData,
              (base::OnceCallback<void(DataTypeSet)>),
              (const override));
  MOCK_METHOD(void,
              HasUnsyncedItemsForTest,
              (base::OnceCallback<void(bool)>),
              (const override));
  MOCK_METHOD(void,
              GetThrottledDataTypesForTest,
              (base::OnceCallback<void(DataTypeSet)>),
              (const override));
  MOCK_METHOD(void,
              RequestBufferedProtocolEventsAndEnableForwarding,
              (),
              (override));
  MOCK_METHOD(void, DisableProtocolEventForwarding, (), (override));
  MOCK_METHOD(void, OnCookieJarChanged, (bool, base::OnceClosure), (override));
  MOCK_METHOD(bool, IsNextPollTimeInThePast, (), (const override));
  MOCK_METHOD(void, ClearNigoriDataForMigration, (), (override));
  MOCK_METHOD(void, GetNigoriNodeForDebugging, (AllNodesCallback), (override));
  MOCK_METHOD(void, RecordNigoriMemoryUsageAndCountsHistograms, (), (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_SYNC_ENGINE_H_
